#include "HexEditor.h"

#include <QAbstractTableModel>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollBar>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace {

class HexModel : public QAbstractTableModel {
public:
    explicit HexModel(std::vector<unsigned char>* bytes, QObject* parent = nullptr)
        : QAbstractTableModel(parent), bytes_(bytes) {
    }

    void reset() {
        beginResetModel();
        endResetModel();
    }

    int rowCount(const QModelIndex& = QModelIndex()) const override {
        return bytes_->empty() ? 0 : static_cast<int>((bytes_->size() + 15) / 16);
    }

    int columnCount(const QModelIndex& = QModelIndex()) const override { return 17; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid()) {
            return {};
        }
        if (role == Qt::TextAlignmentRole) {
            return static_cast<int>(Qt::AlignCenter);
        }
        if (role != Qt::DisplayRole && role != Qt::EditRole) {
            return {};
        }
        const int row = index.row();
        if (index.column() == 0) {
            return role == Qt::EditRole ? QVariant() : QString("%1").arg(static_cast<qulonglong>(row * 16), 8, 16, QChar('0')).toUpper();
        }
        const size_t offset = static_cast<size_t>(row * 16 + index.column() - 1);
        if (offset >= bytes_->size()) {
            return {};
        }
        return QString("%1").arg((*bytes_)[offset], 2, 16, QChar('0')).toUpper();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) {
            return {};
        }
        if (orientation == Qt::Horizontal) {
            return section == 0 ? "Offset" : QString::number(section - 1, 16).toUpper();
        }
        return section;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (index.column() > 0) {
            const size_t offset = static_cast<size_t>(index.row() * 16 + index.column() - 1);
            if (offset < bytes_->size()) {
                flags |= Qt::ItemIsEditable;
            }
        }
        return flags;
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override {
        if (role != Qt::EditRole || index.column() == 0) {
            return false;
        }
        const size_t offset = static_cast<size_t>(index.row() * 16 + index.column() - 1);
        if (offset >= bytes_->size()) {
            return false;
        }
        bool ok = false;
        const uint byte = value.toString().toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            return false;
        }
        (*bytes_)[offset] = static_cast<unsigned char>(byte);
        emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    }

private:
    std::vector<unsigned char>* bytes_;
};

class AsciiModel : public QAbstractTableModel {
public:
    explicit AsciiModel(std::vector<unsigned char>* bytes, QObject* parent = nullptr)
        : QAbstractTableModel(parent), bytes_(bytes) {
    }

    void reset() {
        beginResetModel();
        endResetModel();
    }

    int rowCount(const QModelIndex& = QModelIndex()) const override {
        return bytes_->empty() ? 0 : static_cast<int>((bytes_->size() + 15) / 16);
    }

    int columnCount(const QModelIndex& = QModelIndex()) const override { return 1; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid()) {
            return {};
        }
        if (role == Qt::TextAlignmentRole) {
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (role != Qt::DisplayRole) {
            return {};
        }
        QString text;
        text.reserve(16);
        const size_t start = static_cast<size_t>(index.row() * 16);
        const size_t end = std::min(start + 16, bytes_->size());
        for (size_t i = start; i < end; ++i) {
            const unsigned char c = (*bytes_)[i];
            text += c >= 0x20 && c <= 0x7E ? QChar(c) : QChar(u'·');
        }
        return text;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section == 0) {
            return "ASCII";
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

private:
    std::vector<unsigned char>* bytes_;
};

}

HexEditor::HexEditor(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* controls = new QHBoxLayout;
    offsetEdit_ = new QLineEdit(this);
    offsetEdit_->setMaximumWidth(140);
    offsetEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    goButton_ = new QPushButton(this);
    saveButton_ = new QPushButton(this);
    saveButton_->setEnabled(false);
    controls->addWidget(offsetEdit_);
    controls->addWidget(goButton_);
    controls->addSpacing(8);
    controls->addWidget(saveButton_);
    controls->addStretch();
    layout->addLayout(controls);

    auto* panels = new QHBoxLayout;
    panels->setSpacing(10);
    hexModel_ = new HexModel(&bytes_, this);
    asciiModel_ = new AsciiModel(&bytes_, this);
    hexView_ = new QTableView(this);
    asciiView_ = new QTableView(this);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (QTableView* view : { hexView_, asciiView_ }) {
        view->setFont(mono);
        view->verticalHeader()->hide();
        view->verticalHeader()->setDefaultSectionSize(22);
        view->setShowGrid(false);
        view->setAlternatingRowColors(false);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->horizontalHeader()->setStretchLastSection(false);
    }
    hexView_->setModel(hexModel_);
    asciiView_->setModel(asciiModel_);
    hexView_->setSelectionBehavior(QAbstractItemView::SelectItems);
    asciiView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    asciiView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hexView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    asciiView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    panels->addWidget(hexView_, 1);
    panels->addWidget(asciiView_);
    layout->addLayout(panels, 1);

    asciiView_->setFixedWidth(190);
    retranslate(false);

    connect(goButton_, &QPushButton::clicked, this, &HexEditor::goToOffset);
    connect(offsetEdit_, &QLineEdit::returnPressed, this, &HexEditor::goToOffset);
    connect(saveButton_, &QPushButton::clicked, this, &HexEditor::saveAs);
    connect(hexModel_, &QAbstractTableModel::dataChanged, this, &HexEditor::bytesChanged);
    connect(hexView_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (!syncing_) {
            syncing_ = true;
            asciiView_->verticalScrollBar()->setValue(value);
            syncing_ = false;
        }
    });
    connect(asciiView_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (!syncing_) {
            syncing_ = true;
            hexView_->verticalScrollBar()->setValue(value);
            syncing_ = false;
        }
    });
    connect(hexView_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& current) {
        if (!syncing_ && current.isValid()) {
            syncing_ = true;
            asciiView_->selectRow(current.row());
            syncing_ = false;
        }
    });
}

void HexEditor::setBytes(const std::vector<unsigned char>& bytes) {
    bytes_ = bytes;
    static_cast<HexModel*>(hexModel_)->reset();
    static_cast<AsciiModel*>(asciiModel_)->reset();
    modified_ = false;
    saveButton_->setEnabled(false);

    const int charWidth = hexView_->fontMetrics().horizontalAdvance(QLatin1Char('0'));
    hexView_->setColumnWidth(0, charWidth * 12);
    for (int column = 1; column <= 16; ++column) {
        hexView_->setColumnWidth(column, charWidth * 4);
    }
}

void HexEditor::retranslate(const bool russian) {
    russian_ = russian;
    offsetEdit_->setPlaceholderText(russian ? QString::fromUtf8("Смещение (hex)") : "Offset (hex)");
    offsetEdit_->setToolTip(russian ? QString::fromUtf8("Перейти к смещению в файле") : "Go to a hexadecimal file offset");
    goButton_->setText(russian ? QString::fromUtf8("Перейти") : "Go");
    saveButton_->setText(russian ? QString::fromUtf8("Сохранить как…") : "Save as…");
}

void HexEditor::goToOffset() {
    bool ok = false;
    QString text = offsetEdit_->text().trimmed();
    if (text.startsWith("0x", Qt::CaseInsensitive)) {
        text.remove(0, 2);
    }
    const qulonglong offset = text.toULongLong(&ok, 16);
    if (!ok || bytes_.empty()) {
        return;
    }
    const int row = static_cast<int>(std::min<qulonglong>(offset / 16, (bytes_.size() - 1) / 16));
    const int column = static_cast<int>(std::min<qulonglong>(offset % 16, 15)) + 1;
    const QModelIndex index = hexModel_->index(row, static_cast<int>(column));
    hexView_->setCurrentIndex(index);
    hexView_->scrollTo(index, QAbstractItemView::PositionAtCenter);
}

void HexEditor::bytesChanged() {
    modified_ = true;
    saveButton_->setEnabled(true);
    static_cast<AsciiModel*>(asciiModel_)->reset();
}

bool HexEditor::saveAs() {
    if (bytes_.empty()) {
        return false;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save binary file"), QString(), tr("Binary files (*.bin *.exe *.dll);;All files (*)"));
    if (path.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Save error"), tr("Could not open the file for writing."));
        return false;
    }
    constexpr size_t chunkSize = 1024 * 1024;
    for (size_t offset = 0; offset < bytes_.size(); offset += chunkSize) {
        const size_t size = std::min(chunkSize, bytes_.size() - offset);
        const qint64 written = file.write(reinterpret_cast<const char*>(bytes_.data() + offset), static_cast<qint64>(size));
        if (written != static_cast<qint64>(size)) {
            QMessageBox::critical(this, tr("Save error"), tr("Could not write the complete file."));
            return false;
        }
    }
    modified_ = false;
    saveButton_->setEnabled(false);
    return true;
}
