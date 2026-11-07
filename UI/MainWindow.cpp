#include "MainWindow.h"
#include "HexEditor.h"
#include "Disasm/disasm.h"

#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

namespace {

QLineEdit* readOnlyValue(QWidget* parent) {
    auto* value = new QLineEdit(parent);
    value->setReadOnly(true);
    value->setFrame(false);
    value->setCursorPosition(0);
    return value;
}

void configureTable(QTableWidget* table) {
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->setWordWrap(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

QTableWidget* makeTable(int columns, QWidget* parent) {
    auto* table = new QTableWidget(0, columns, parent);
    configureTable(table);
    return table;
}

QString sectionCharacteristics(const DWORD flags) {
    QStringList parts;
    if (flags & IMAGE_SCN_CNT_CODE) parts << "CODE";
    if (flags & IMAGE_SCN_CNT_INITIALIZED_DATA) parts << "IDATA";
    if (flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) parts << "UDATA";
    if (flags & IMAGE_SCN_MEM_EXECUTE) parts << "EXEC";
    if (flags & IMAGE_SCN_MEM_READ) parts << "READ";
    if (flags & IMAGE_SCN_MEM_WRITE) parts << "WRITE";
    if (flags & IMAGE_SCN_MEM_DISCARDABLE) parts << "DISCARDABLE";
    if (flags & IMAGE_SCN_MEM_SHARED) parts << "SHARED";
    return parts.join(" · ");
}

QPalette applicationPalette(const bool dark) {
    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#121215"));
        palette.setColor(QPalette::WindowText, QColor("#e4e4e8"));
        palette.setColor(QPalette::Base, QColor("#0e0e11"));
        palette.setColor(QPalette::AlternateBase, QColor("#202026"));
        palette.setColor(QPalette::ToolTipBase, QColor("#18181c"));
        palette.setColor(QPalette::ToolTipText, QColor("#e4e4e8"));
        palette.setColor(QPalette::Text, QColor("#e4e4e8"));
        palette.setColor(QPalette::Button, QColor("#18181c"));
        palette.setColor(QPalette::ButtonText, QColor("#e4e4e8"));
        palette.setColor(QPalette::PlaceholderText, QColor("#8e8e99"));
        palette.setColor(QPalette::Highlight, QColor("#9487c7"));
        palette.setColor(QPalette::HighlightedText, QColor("#15131d"));
        palette.setColor(QPalette::Mid, QColor("#2a2a31"));
        palette.setColor(QPalette::Dark, QColor("#0e0e11"));
        palette.setColor(QPalette::Light, QColor("#2a2a31"));
    } else {
        palette.setColor(QPalette::Window, QColor("#f2f2f4"));
        palette.setColor(QPalette::WindowText, QColor("#232329"));
        palette.setColor(QPalette::Base, QColor("#fbfbfc"));
        palette.setColor(QPalette::AlternateBase, QColor("#eaeaee"));
        palette.setColor(QPalette::ToolTipBase, QColor("#fbfbfc"));
        palette.setColor(QPalette::ToolTipText, QColor("#232329"));
        palette.setColor(QPalette::Text, QColor("#232329"));
        palette.setColor(QPalette::Button, QColor("#fbfbfc"));
        palette.setColor(QPalette::ButtonText, QColor("#232329"));
        palette.setColor(QPalette::PlaceholderText, QColor("#63636e"));
        palette.setColor(QPalette::Highlight, QColor("#5e5498"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Mid, QColor("#d8d8de"));
        palette.setColor(QPalette::Dark, QColor("#63636e"));
        palette.setColor(QPalette::Light, QColor("#ffffff"));
    }
    return palette;
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    buildUi();
    resize(1120, 760);
    setMinimumSize(820, 600);
}

MainWindow::~MainWindow() = default;

QString MainWindow::text(const char* english, const char* russian) const {
    return language_ == "ru" ? QString::fromUtf8(russian) : QString::fromUtf8(english);
}

QString MainWindow::formatHex(const qulonglong value, const int digits) const {
    QString number = QString::number(value, 16).toUpper();
    if (digits > 0 && number.size() < digits) {
        number = QString(digits - number.size(), QChar('0')) + number;
    }
    return "0x" + number;
}

QString MainWindow::formatSize(const qulonglong bytes) const {
    if (units_ == "bytes") {
        return QLocale().toString(bytes) + " B";
    }
    if (units_ == "kb") {
        return QString::number(static_cast<double>(bytes) / 1024.0, 'f', 2) + " KB";
    }
    if (units_ == "mb") {
        return QString::number(static_cast<double>(bytes) / 1048576.0, 'f', 2) + " MB";
    }
    if (bytes >= 1048576) {
        return QString::number(static_cast<double>(bytes) / 1048576.0, 'f', 2) + " MB";
    }
    if (bytes >= 1024) {
        return QString::number(static_cast<double>(bytes) / 1024.0, 'f', 2) + " KB";
    }
    return QString::number(bytes) + " B";
}

void MainWindow::buildUi() {
    setWindowTitle("FastBinary");
    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    setCentralWidget(central);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(12, 8, 12, 8);
    openButton_ = new QPushButton(central);
    fileLabel_ = new QLabel(central);
    fileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topBar->addWidget(openButton_);
    topBar->addWidget(fileLabel_, 1);
    rootLayout->addLayout(topBar);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    auto* sidebar = new QWidget(central);
    sidebar->setFixedWidth(172);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(8, 8, 8, 8);
    sideLayout->setSpacing(3);
    for (int i = 0; i < 9; ++i) {
        if (i == 7) {
            sideLayout->addStretch();
        }
        auto* button = new QToolButton(sidebar);
        button->setProperty("nav", true);
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumHeight(34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        navButtons_[i] = button;
        sideLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, i] { stack_->setCurrentIndex(i); });
    }
    navButtons_[0]->setChecked(true);
    body->addWidget(sidebar);

    stack_ = new QStackedWidget(central);
    for (int i = 0; i < 9; ++i) {
        stack_->addWidget(createPage(i));
    }
    body->addWidget(stack_, 1);
    rootLayout->addLayout(body, 1);

    statusLabel_ = new QLabel(central);
    statusBar()->addWidget(statusLabel_, 1);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::openFileDialog);
    connect(themeCombo_, &QComboBox::currentIndexChanged, this, &MainWindow::applyTheme);
    connect(languageCombo_, &QComboBox::currentIndexChanged, this, [this] {
        language_ = languageCombo_->currentData().toString();
        retranslateUi();
    });
    connect(unitsCombo_, &QComboBox::currentIndexChanged, this, [this] {
        units_ = unitsCombo_->currentData().toString();
        applyUnits();
    });
    connect(stringsFilter_, &QLineEdit::textChanged, this, &MainWindow::filterStrings);
    connect(importsList_, &QListWidget::currentRowChanged, this, &MainWindow::updateImportFunctions);
    connect(disasmSpacing_, &QSlider::valueChanged, this, [this](int value) {
        disasmSpacingValue_->setText(QString::number(value) + "px");
        if (disasmTable_) {
            const int base = disasmTable_->fontMetrics().horizontalAdvance("0x0000000000000000") + 12;
            disasmTable_->setColumnWidth(0, base + value);
        }
    });

    setAcceptDrops(true);
    retranslateUi();
    applyTheme();
    updateStatus();
}

QWidget* MainWindow::createPage(const int index) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(10);
    auto* title = new QLabel(page);
    title->setProperty("pageTitle", true);
    pageTitles_[index] = title;
    layout->addWidget(title);

    switch (index) {
        case 0: {
            auto* form = new QFormLayout;
            form->setLabelAlignment(Qt::AlignLeft);
            form->setHorizontalSpacing(24);
            form->setVerticalSpacing(8);
            for (int i = 0; i < 10; ++i) {
                quickLabels_[i] = new QLabel(page);
                quickValues_[i] = readOnlyValue(page);
                quickValues_[i]->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
                form->addRow(quickLabels_[i], quickValues_[i]);
            }
            layout->addLayout(form);
            layout->addStretch();
            break;
        }
        case 1: {
            auto* entropy = new QHBoxLayout;
            totalEntropyLabel_ = new QLabel(page);
            totalEntropyValue_ = readOnlyValue(page);
            totalEntropyValue_->setMaximumWidth(90);
            totalEntropyBar_ = new QProgressBar(page);
            totalEntropyBar_->setRange(0, 800);
            totalEntropyBar_->setTextVisible(false);
            totalEntropyBar_->setMaximumHeight(6);
            entropy->addWidget(totalEntropyLabel_);
            entropy->addWidget(totalEntropyValue_);
            entropy->addWidget(totalEntropyBar_, 1);
            layout->addLayout(entropy);
            sectionsTable_ = makeTable(7, page);
            layout->addWidget(sectionsTable_, 1);
            break;
        }
        case 2: {
            auto* controls = new QHBoxLayout;
            stringsFilter_ = new QLineEdit(page);
            stringsFilter_->setMaximumWidth(280);
            stringsCount_ = new QLabel(page);
            controls->addWidget(stringsFilter_);
            controls->addWidget(stringsCount_);
            controls->addStretch();
            layout->addLayout(controls);
            stringsTable_ = makeTable(2, page);
            layout->addWidget(stringsTable_, 1);
            break;
        }
        case 3: {
            disasmEntryLabel_ = new QLabel(page);
            layout->addWidget(disasmEntryLabel_);
            auto* spacing = new QHBoxLayout;
            disasmSpacingLabel_ = new QLabel(page);
            disasmSpacing_ = new QSlider(Qt::Horizontal, page);
            disasmSpacing_->setRange(8, 64);
            disasmSpacing_->setSingleStep(4);
            disasmSpacing_->setValue(24);
            disasmSpacing_->setMaximumWidth(180);
            disasmSpacingValue_ = new QLabel(page);
            spacing->addWidget(disasmSpacingLabel_);
            spacing->addWidget(disasmSpacing_);
            spacing->addWidget(disasmSpacingValue_);
            spacing->addStretch();
            layout->addLayout(spacing);
            disasmStatus_ = new QLabel(page);
            disasmStatus_->hide();
            layout->addWidget(disasmStatus_);
            disasmTable_ = makeTable(3, page);
            layout->addWidget(disasmTable_, 1);
            break;
        }
        case 4: {
            auto* columns = new QHBoxLayout;
            importsList_ = new QListWidget(page);
            importsList_->setMaximumWidth(240);
            columns->addWidget(importsList_);
            auto* right = new QVBoxLayout;
            importsCount_ = new QLabel(page);
            importsTable_ = makeTable(2, page);
            right->addWidget(importsCount_);
            right->addWidget(importsTable_, 1);
            columns->addLayout(right, 1);
            layout->addLayout(columns, 1);
            break;
        }
        case 5: {
            tlsEmpty_ = new QLabel(page);
            tlsEmpty_->hide();
            layout->addWidget(tlsEmpty_);
            tlsContent_ = new QWidget(page);
            auto* contentLayout = new QVBoxLayout(tlsContent_);
            contentLayout->setContentsMargins(0, 0, 0, 0);
            auto* form = new QFormLayout;
            for (int i = 0; i < 6; ++i) {
                tlsLabels_[i] = new QLabel(tlsContent_);
                tlsValues_[i] = readOnlyValue(tlsContent_);
                tlsValues_[i]->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
                form->addRow(tlsLabels_[i], tlsValues_[i]);
            }
            contentLayout->addLayout(form);
            tlsCallbacksLabel_ = new QLabel(tlsContent_);
            tlsCallbacks_ = new QListWidget(tlsContent_);
            tlsCallbacks_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            contentLayout->addWidget(tlsCallbacksLabel_);
            contentLayout->addWidget(tlsCallbacks_, 1);
            layout->addWidget(tlsContent_, 1);
            break;
        }
        case 6: {
            resourcesTable_ = makeTable(5, page);
            layout->addWidget(resourcesTable_, 1);
            break;
        }
        case 7: {
            hexEditor_ = new HexEditor(page);
            layout->addWidget(hexEditor_, 1);
            break;
        }
        case 8: {
            auto* form = new QFormLayout;
            themeLabel_ = new QLabel(page);
            languageLabel_ = new QLabel(page);
            unitsLabel_ = new QLabel(page);
            themeCombo_ = new QComboBox(page);
            languageCombo_ = new QComboBox(page);
            unitsCombo_ = new QComboBox(page);
            themeCombo_->addItem(QString(), "auto");
            themeCombo_->addItem(QString(), "dark");
            themeCombo_->addItem(QString(), "light");
            languageCombo_->addItem("English", "en");
            languageCombo_->addItem("Русский", "ru");
            unitsCombo_->addItem(QString(), "auto");
            unitsCombo_->addItem(QString(), "bytes");
            unitsCombo_->addItem("KB", "kb");
            unitsCombo_->addItem("MB", "mb");
            form->addRow(themeLabel_, themeCombo_);
            form->addRow(languageLabel_, languageCombo_);
            form->addRow(unitsLabel_, unitsCombo_);
            layout->addLayout(form);
            layout->addStretch();
            break;
        }
    }
    return page;
}

bool MainWindow::loadFile(const QString& path) {
    auto memoryResult = Memory::load(path.toStdWString());
    if (!memoryResult) {
        QMessageBox::critical(this, text("Open error", "Ошибка открытия"), QString::fromStdString(memoryResult.error()));
        return false;
    }
    auto peResult = PE::create(memoryResult->getBuffer());
    if (!peResult) {
        QMessageBox::critical(this, text("PE error", "Ошибка PE"), QString::fromStdString(peResult.error()));
        return false;
    }

    memory_ = std::make_unique<Memory>(std::move(*memoryResult));
    pe_ = std::make_unique<PE>(std::move(*peResult));
    filePath_ = path;
    strings_ = PE::extractStrings(memory_->getBuffer());
    const auto buffer = memory_->getBuffer();
    hexEditor_->setBytes(std::vector<unsigned char>(buffer.begin(), buffer.end()));

    const QFileInfo info(path);
    fileLabel_->setText(info.absoluteFilePath());
    setWindowTitle("FastBinary — " + info.fileName());
    updateAll();
    stack_->setCurrentIndex(0);
    navButtons_[0]->setChecked(true);
    return true;
}

void MainWindow::updateAll() {
    if (!memory_ || !pe_) {
        return;
    }
    updateQuickInfo();
    updateSections();
    updateStrings();
    updateDisasm();
    updateImports();
    updateTls();
    updateResources();
    updateStatus();
}

void MainWindow::updateQuickInfo() {
    quickValues_[0]->setText(pe_->b_IsPE64() ? "PE32+ (64-bit)" : "PE32 (32-bit)");
    quickValues_[1]->setText(QString::fromUtf8(pe_->getArchitectureName().data(), static_cast<int>(pe_->getArchitectureName().size())));
    quickValues_[2]->setText(QString::fromStdString(pe_->getBuildDateTime()));
    quickValues_[3]->setText(text("Not checked", "Не проверялся"));
    quickValues_[4]->setText(QString::fromStdString(memory_->getHashMD5()));
    quickValues_[5]->setText(QString::fromStdString(memory_->getHash256()));
    quickValues_[6]->setText(formatHex(pe_->qw_getBaseAddress()));
    quickValues_[7]->setText(formatHex(pe_->getEntryPointRva()) + " (RVA)");
    quickValues_[8]->setText(subsystemName());
    quickValues_[9]->setText(formatSize(memory_->getBuffer().size()));
    for (QLineEdit* value : quickValues_) {
        value->setCursorPosition(0);
    }
}

QString MainWindow::subsystemName() const {
    switch (pe_->getSubsystem()) {
        case IMAGE_SUBSYSTEM_NATIVE: return "Native";
        case IMAGE_SUBSYSTEM_WINDOWS_GUI: return "Windows GUI";
        case IMAGE_SUBSYSTEM_WINDOWS_CUI: return "Windows Console";
        case IMAGE_SUBSYSTEM_OS2_CUI: return "OS/2 Console";
        case IMAGE_SUBSYSTEM_POSIX_CUI: return "POSIX Console";
        case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI: return "Windows CE GUI";
        case IMAGE_SUBSYSTEM_EFI_APPLICATION: return "EFI Application";
        case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: return "EFI Boot Driver";
        case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER: return "EFI Runtime Driver";
        default: return text("Unknown", "Неизвестно") + QString(" (%1)").arg(pe_->getSubsystem());
    }
}

void MainWindow::updateSections() {
    const double entropy = PE::calculateEntropy(memory_->getBuffer());
    totalEntropyValue_->setText(QString::number(entropy, 'f', 2) + " / 8");
    totalEntropyBar_->setValue(static_cast<int>(entropy * 100));

    const auto& sections = pe_->getSections();
    sectionsTable_->setRowCount(static_cast<int>(sections.size()));
    for (int row = 0; row < static_cast<int>(sections.size()); ++row) {
        const PE::Section& section = sections[row];
        const QStringList values {
            QString::fromStdString(section.name),
            formatHex(section.virtual_size),
            formatHex(section.virtual_address),
            formatHex(section.raw_size),
            formatHex(section.raw_offset),
            QString::number(section.entropy, 'f', 2),
            sectionCharacteristics(section.characteristics)
        };
        for (int column = 0; column < values.size(); ++column) {
            sectionsTable_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
    sectionsTable_->resizeColumnsToContents();
}

void MainWindow::updateStrings() {
    stringsTable_->setRowCount(static_cast<int>(strings_.size()));
    for (int row = 0; row < static_cast<int>(strings_.size()); ++row) {
        const PE::StringEntry& entry = strings_[row];
        stringsTable_->setItem(row, 0, new QTableWidgetItem(formatHex(entry.offset, 8)));
        stringsTable_->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(entry.text.data(), static_cast<int>(entry.text.size()))));
    }
    filterStrings();
}

void MainWindow::filterStrings() {
    const QString filter = stringsFilter_ ? stringsFilter_->text() : QString();
    int visible = 0;
    for (int row = 0; row < stringsTable_->rowCount(); ++row) {
        QTableWidgetItem* item = stringsTable_->item(row, 1);
        const bool show = !item || item->text().contains(filter, Qt::CaseInsensitive);
        stringsTable_->setRowHidden(row, !show);
        if (show) {
            ++visible;
        }
    }
    stringsCount_->setText(QString("%1 / %2").arg(visible).arg(stringsTable_->rowCount()));
}

void MainWindow::updateDisasm() {
    disasmTable_->setRowCount(0);
    disasmStatus_->hide();
    disasmEntryLabel_->setText(text("Entry point", "Точка входа") + " · " + formatHex(pe_->getEntryPointRva()) + " (RVA)");
    const auto offset = pe_->entryPointOffset();
    if (!offset || *offset >= memory_->getBuffer().size()) {
        disasmStatus_->setText(text("Entry point bytes are not available in the file.", "Байты точки входа недоступны в файле."));
        disasmStatus_->show();
        return;
    }
    auto disasm = Disasm::create(pe_->b_IsPE64() ? Disasm::Mode::X64 : Disasm::Mode::X86);
    if (!disasm) {
        disasmStatus_->setText(QString::fromStdString(disasm.error()));
        disasmStatus_->show();
        return;
    }
    const auto buffer = memory_->getBuffer();
    const size_t size = std::min<size_t>(512, buffer.size() - *offset);
    auto instructions = disasm->disassemble(buffer.subspan(*offset, size), pe_->qw_getBaseAddress() + pe_->getEntryPointRva(), 200);
    if (!instructions) {
        disasmStatus_->setText(QString::fromStdString(instructions.error()));
        disasmStatus_->show();
        return;
    }
    disasmTable_->setRowCount(static_cast<int>(instructions->size()));
    for (int row = 0; row < static_cast<int>(instructions->size()); ++row) {
        const Disasm::Instruction& instruction = (*instructions)[row];
        disasmTable_->setItem(row, 0, new QTableWidgetItem(formatHex(instruction.address)));
        disasmTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(instruction.bytes)));
        QString mnemonic = QString::fromStdString(instruction.mnemonic);
        if (!instruction.operands.empty()) {
            mnemonic += " " + QString::fromStdString(instruction.operands);
        }
        disasmTable_->setItem(row, 2, new QTableWidgetItem(mnemonic));
    }
    disasmTable_->resizeColumnToContents(1);
    const int base = disasmTable_->fontMetrics().horizontalAdvance("0x0000000000000000") + 12;
    disasmTable_->setColumnWidth(0, base + disasmSpacing_->value());
}

void MainWindow::updateImports() {
    importsList_->clear();
    for (const PE::ImportLibrary& library : pe_->getImports()) {
        importsList_->addItem(QString::fromStdString(library.name));
    }
    if (importsList_->count() > 0) {
        importsList_->setCurrentRow(0);
    } else {
        updateImportFunctions(-1);
    }
}

void MainWindow::updateImportFunctions(const int row) {
    importsTable_->setRowCount(0);
    if (row < 0 || row >= static_cast<int>(pe_->getImports().size())) {
        importsCount_->setText(text("No imports", "Импорт отсутствует"));
        return;
    }
    const PE::ImportLibrary& library = pe_->getImports()[row];
    importsTable_->setRowCount(static_cast<int>(library.functions.size()));
    for (int index = 0; index < static_cast<int>(library.functions.size()); ++index) {
        const PE::ImportFunction& function = library.functions[index];
        importsTable_->setItem(index, 0, new QTableWidgetItem(QString::number(index)));
        importsTable_->setItem(index, 1, new QTableWidgetItem(function.by_ordinal
            ? QString("#%1").arg(function.ordinal)
            : QString::fromStdString(function.name)));
    }
    importsCount_->setText(QString("%1 %2").arg(library.functions.size()).arg(text("functions", "функций")));
}

void MainWindow::updateTls() {
    const PE::TlsInfo& tls = pe_->getTls();
    tlsEmpty_->setVisible(!tls.present);
    tlsContent_->setVisible(tls.present);
    tlsEmpty_->setText(text("No TLS directory.", "TLS-директория отсутствует."));
    if (!tls.present) {
        return;
    }
    const qulonglong values[] {
        tls.start_raw_data,
        tls.end_raw_data,
        tls.address_of_index,
        tls.address_of_callbacks,
        tls.zero_fill,
        tls.characteristics
    };
    for (int i = 0; i < 6; ++i) {
        tlsValues_[i]->setText(formatHex(values[i]));
    }
    tlsCallbacks_->clear();
    for (const QWORD callback : tls.callbacks) {
        tlsCallbacks_->addItem(formatHex(callback));
    }
}

void MainWindow::updateResources() {
    const auto& resources = pe_->getResources();
    resourcesTable_->setRowCount(static_cast<int>(resources.size()));
    for (int row = 0; row < static_cast<int>(resources.size()); ++row) {
        const PE::Resource& resource = resources[row];
        resourcesTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(resource.type)));
        resourcesTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(resource.name)));
        resourcesTable_->setItem(row, 2, new QTableWidgetItem(QString::number(resource.language)));
        auto* size = new QTableWidgetItem(formatSize(resource.size));
        size->setData(Qt::UserRole, static_cast<qulonglong>(resource.size));
        resourcesTable_->setItem(row, 3, size);
        resourcesTable_->setItem(row, 4, new QTableWidgetItem(formatHex(resource.offset)));
    }
}

void MainWindow::updateStatus() {
    if (!memory_) {
        statusLabel_->setText(text("Open a PE file to begin analysis.", "Откройте PE-файл для начала анализа."));
        return;
    }
    statusLabel_->setText(QFileInfo(filePath_).fileName() + " · " + formatSize(memory_->getBuffer().size()));
}

void MainWindow::applyUnits() {
    if (memory_ && pe_) {
        updateQuickInfo();
        updateResources();
        updateStatus();
    }
}

void MainWindow::applyTheme() {
    const QString theme = themeCombo_->currentData().toString();
    if (theme == "dark") {
        qApp->setPalette(applicationPalette(true));
    } else if (theme == "light") {
        qApp->setPalette(applicationPalette(false));
    } else {
        qApp->setPalette(qApp->style()->standardPalette());
    }
    qApp->setStyleSheet(R"(
        QToolButton[nav="true"] {
            text-align: left;
            padding: 7px 9px;
            border: 0;
            border-radius: 5px;
            font-family: "Cascadia Mono", Consolas, monospace;
            font-size: 12px;
        }
        QToolButton[nav="true"]:hover { background: palette(alternate-base); }
        QToolButton[nav="true"]:checked {
            background: palette(alternate-base);
            color: palette(highlight);
            border-left: 2px solid palette(highlight);
            font-weight: 600;
        }
        QLabel[pageTitle="true"] {
            font-family: "Cascadia Mono", Consolas, monospace;
            font-weight: 600;
        }
        QLineEdit[readOnly="true"] { background: transparent; border: 0; }
    )");
}

void MainWindow::retranslateUi() {
    const char* englishNames[] { "Quick Info", "Sections", "Strings", "Disasm", "Imports", "TLS", "Resources", "HEX", "Settings" };
    const char* russianNames[] { "Сводка", "Секции", "Строки", "Дизассемблер", "Импорт", "TLS", "Ресурсы", "HEX", "Настройки" };
    for (int i = 0; i < 9; ++i) {
        const QString name = language_ == "ru" ? QString::fromUtf8(russianNames[i]) : QString::fromUtf8(englishNames[i]);
        navButtons_[i]->setText(name);
        pageTitles_[i]->setText(name);
    }
    openButton_->setText(text("Open…", "Открыть…"));

    const char* quickEnglish[] { "Bitness", "Architecture", "Build date", "VirusTotal", "MD5", "SHA256", "ImageBase", "Entry point", "Subsystem", "File size" };
    const char* quickRussian[] { "Битность", "Архитектура", "Дата сборки", "VirusTotal", "MD5", "SHA256", "ImageBase", "Точка входа", "Подсистема", "Размер файла" };
    for (int i = 0; i < 10; ++i) {
        quickLabels_[i]->setText(language_ == "ru" ? QString::fromUtf8(quickRussian[i]) : QString::fromUtf8(quickEnglish[i]));
    }
    totalEntropyLabel_->setText(text("Total entropy", "Общая энтропия"));
    sectionsTable_->setHorizontalHeaderLabels({ "Name", "VirtualSize", "VirtualAddress", "RawSize", "RawPtr", "Entropy", "Characteristics" });
    stringsFilter_->setPlaceholderText(text("Filter strings…", "Фильтр строк…"));
    stringsTable_->setHorizontalHeaderLabels({ text("Offset", "Смещение"), text("String", "Строка") });
    disasmSpacingLabel_->setText(text("Spacing", "Интервал"));
    disasmSpacingValue_->setText(QString::number(disasmSpacing_->value()) + "px");
    disasmTable_->setHorizontalHeaderLabels({ text("Address", "Адрес"), text("Bytes", "Байты"), text("Instruction", "Инструкция") });
    importsTable_->setHorizontalHeaderLabels({ "#", text("Function", "Функция") });
    resourcesTable_->setHorizontalHeaderLabels({ text("Type", "Тип"), text("Name", "Имя"), "Lang", text("Size", "Размер"), text("Offset", "Смещение") });

    const char* tlsEnglish[] { "StartAddressOfRawData", "EndAddressOfRawData", "AddressOfIndex", "AddressOfCallBacks", "SizeOfZeroFill", "Characteristics" };
    for (int i = 0; i < 6; ++i) {
        tlsLabels_[i]->setText(tlsEnglish[i]);
    }
    tlsCallbacksLabel_->setText(text("Callbacks", "Обратные вызовы"));
    themeLabel_->setText(text("Theme", "Тема"));
    languageLabel_->setText(text("Language", "Язык"));
    unitsLabel_->setText(text("Size units", "Единицы размера"));
    themeCombo_->setItemText(0, text("Auto", "Авто"));
    themeCombo_->setItemText(1, text("Dark", "Тёмная"));
    themeCombo_->setItemText(2, text("Light", "Светлая"));
    unitsCombo_->setItemText(0, text("Auto", "Авто"));
    unitsCombo_->setItemText(1, text("Bytes", "Байты"));
    hexEditor_->retranslate(language_ == "ru");
    if (pe_) {
        updateQuickInfo();
        updateDisasm();
        updateImportFunctions(importsList_->currentRow());
        updateTls();
    }
    updateStatus();
}

void MainWindow::openFileDialog() {
    const QString path = QFileDialog::getOpenFileName(this,
        text("Open PE file", "Открыть PE-файл"),
        filePath_,
        "PE files (*.exe *.dll *.sys *.ocx *.cpl);;All files (*)");
    if (!path.isEmpty()) {
        loadFile(path);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        loadFile(urls.first().toLocalFile());
    }
}
