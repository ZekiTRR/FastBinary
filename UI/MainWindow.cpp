#include "MainWindow.h"
#include "HexEditor.h"
#include "Disasm/disasm.h"

#include <QApplication>
#include <QCloseEvent>
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
#include <QDir>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QStyleFactory>
#include <QWheelEvent>

#include <algorithm>

namespace {

// В smoke-тестах настройки не читаем и не пишем, чтобы не трогать профиль пользователя.
bool settingsEnabled() {
    return !qEnvironmentVariableIsSet("FB_DESKTOP_TEST");
}

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
        // Светлая тема: белый контент на холодном сером фоне, индиго-акцент.
        // Текст почти чёрный (контраст ~15:1), акцент читается и на белом, и на сером.
        palette.setColor(QPalette::Window, QColor("#f4f4f6"));
        palette.setColor(QPalette::WindowText, QColor("#1f2128"));
        palette.setColor(QPalette::Base, QColor("#ffffff"));
        palette.setColor(QPalette::AlternateBase, QColor("#ececef"));
        palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
        palette.setColor(QPalette::ToolTipText, QColor("#1f2128"));
        palette.setColor(QPalette::Text, QColor("#1f2128"));
        palette.setColor(QPalette::Button, QColor("#ffffff"));
        palette.setColor(QPalette::ButtonText, QColor("#1f2128"));
        palette.setColor(QPalette::BrightText, QColor("#ffffff"));
        palette.setColor(QPalette::PlaceholderText, QColor("#868892"));
        palette.setColor(QPalette::Link, QColor("#5150d5"));
        palette.setColor(QPalette::Highlight, QColor("#5150d5"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Mid, QColor("#dcdce1"));
        palette.setColor(QPalette::Dark, QColor("#c3c4cc"));
        palette.setColor(QPalette::Light, QColor("#ffffff"));
        // Неактивные виджеты: заметно серее основного текста, без выцветания.
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#a7a9b3"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a7a9b3"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a7a9b3"));
        palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#f1f1f4"));
    }
    return palette;
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    loadSettings();
    buildUi();
    resize(1120, 760);
    setMinimumSize(820, 600);
    if (settingsEnabled() && !savedGeometry_.isEmpty()) {
        restoreGeometry(savedGeometry_);
    }

    // Размер шрифта меняется комбинациями клавиш и Ctrl+колёсико над любым виджетом окна.
    qApp->installEventFilter(this);
    auto* zoomIn = new QShortcut(QKeySequence::ZoomIn, this);
    auto* zoomOut = new QShortcut(QKeySequence::ZoomOut, this);
    auto* zoomReset = new QShortcut(QKeySequence("Ctrl+0"), this);
    // На части раскладок ZoomIn уже равен Ctrl+= — не создаём дубль (Qt посчитал бы его неоднозначным).
    QShortcut* zoomInEqual = QKeySequence(QKeySequence::ZoomIn) == QKeySequence("Ctrl+=")
        ? nullptr : new QShortcut(QKeySequence("Ctrl+="), this);
    for (QShortcut* shortcut : { zoomIn, zoomOut, zoomReset, zoomInEqual }) {
        if (shortcut) {
            shortcut->setContext(Qt::WindowShortcut);
        }
    }
    connect(zoomIn, &QShortcut::activated, this, [this] { changeFontSize(1); });
    if (zoomInEqual) {
        connect(zoomInEqual, &QShortcut::activated, this, [this] { changeFontSize(1); });
    }
    connect(zoomOut, &QShortcut::activated, this, [this] { changeFontSize(-1); });
    connect(zoomReset, &QShortcut::activated, this, [this] {
        fontDelta_ = 0;
        applyFonts();
    });
    applyFonts();
}

MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
}

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
    defaultStyle_ = qApp->style()->objectName();
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
    for (int i = 0; i < 10; ++i) {
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
    for (int i = 0; i < 10; ++i) {
        stack_->addWidget(createPage(i));
    }
    body->addWidget(stack_, 1);
    rootLayout->addLayout(body, 1);

    // Восстанавливаем сохранённые настройки до подключения сигналов — без лишних re-apply.
    if (const int themeIndex = themeCombo_->findData(theme_); themeIndex >= 0) {
        themeCombo_->setCurrentIndex(themeIndex);
    }
    if (const int languageIndex = languageCombo_->findData(language_); languageIndex >= 0) {
        languageCombo_->setCurrentIndex(languageIndex);
    }
    if (const int unitsIndex = unitsCombo_->findData(units_); unitsIndex >= 0) {
        unitsCombo_->setCurrentIndex(unitsIndex);
    }
    if (const int packerIndex = packerCombo_->findData(packer_); packerIndex >= 0) {
        packerCombo_->setCurrentIndex(packerIndex);
    }
    disasmSpacing_->setValue(disasmSpacingSaved_);

    statusLabel_ = new QLabel(central);
    statusBar()->addWidget(statusLabel_, 1);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::openFileDialog);
    connect(stack_, &QStackedWidget::currentChanged, this, [this](const int index) {
        if (pageDirty_[index]) {
            updatePage(index);
        }
    });
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
    connect(packerCombo_, &QComboBox::currentIndexChanged, this, [this] { updatePackers(); });
    connect(packButton_, &QPushButton::clicked, this, &MainWindow::startPack);
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
            for (int i = 0; i < 9; ++i) {
                quickLabels_[i] = new QLabel(page);
                quickLabels_[i]->setProperty("formLabel", true);
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
                tlsLabels_[i]->setProperty("formLabel", true);
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
            // Packers: упаковка открытого файла выбранным инструментом.
            // Xenolith вызывается напрямую через сабкоманду pack — его TUI не запускается.
            packersForm_ = new QFormLayout;
            packersForm_->setLabelAlignment(Qt::AlignLeft);
            packersForm_->setHorizontalSpacing(24);
            packersForm_->setVerticalSpacing(8);
            for (int i = 0; i < 4; ++i) {
                packerLabels_[i] = new QLabel(page);
                packerLabels_[i]->setProperty("formLabel", true);
            }
            packerCombo_ = new QComboBox(page);
            packerCombo_->addItem("Xenolith", "xenolith");
            packerCombo_->addItem("UPX", "upx");
            xenolithProfileCombo_ = new QComboBox(page);
            xenolithProfileCombo_->addItem(QString(), "fast");
            xenolithProfileCombo_->addItem(QString(), "standard");
            xenolithProfileCombo_->addItem(QString(), "max");
            xenolithExportsEdit_ = new QLineEdit(page);
            packerOutputEdit_ = readOnlyValue(page);
            packersForm_->addRow(packerLabels_[0], packerCombo_);
            packersForm_->addRow(packerLabels_[1], xenolithProfileCombo_);
            packersForm_->addRow(packerLabels_[2], xenolithExportsEdit_);
            packersForm_->addRow(packerLabels_[3], packerOutputEdit_);
            layout->addLayout(packersForm_);
            packButton_ = new QPushButton(page);
            layout->addWidget(packButton_);
            packerNoteLabel_ = new QLabel(page);
            packerNoteLabel_->setWordWrap(true);
            layout->addWidget(packerNoteLabel_);
            packerLog_ = new QPlainTextEdit(page);
            packerLog_->setReadOnly(true);
            packerLog_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            layout->addWidget(packerLog_, 1);
            break;
        }
        case 9: {
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
    strings_.clear();
    stringsExtracted_ = false;

    const QFileInfo info(path);
    fileLabel_->setText(info.absoluteFilePath());
    setWindowTitle("FastBinary — " + info.fileName());

    // Тяжёлые вкладки (строки, дизасм, HEX-копия) заполняются лениво — при первом открытии.
    markPagesDirty();
    stack_->setCurrentIndex(0);
    navButtons_[0]->setChecked(true);
    updateCurrentPage();
    updateStatus();
    return true;
}

void MainWindow::markPagesDirty() {
    for (bool& dirty : pageDirty_) {
        dirty = true;
    }
}

void MainWindow::updateCurrentPage() {
    updatePage(stack_->currentIndex());
}

void MainWindow::updatePage(const int index) {
    if (index < 0 || index > 9) {
        return;
    }
    pageDirty_[index] = false;
    switch (index) {
        case 0: updateQuickInfo(); break;
        case 1: updateSections(); break;
        case 2: updateStrings(); break;
        case 3: updateDisasm(); break;
        case 4: updateImports(); break;
        case 5: updateTls(); break;
        case 6: updateResources(); break;
        case 7:
            // HEX-редактор получает свою копию байтов один раз на файл,
            // иначе повторный заход на вкладку стёр бы пользовательские правки.
            if (memory_) {
                const auto buffer = memory_->getBuffer();
                hexEditor_->setBytes(std::vector<unsigned char>(buffer.begin(), buffer.end()));
            }
            break;
        case 8: updatePackers(); break;
        case 9: break; // Настройки заполнять нечем.
    }
}

void MainWindow::updateQuickInfo() {
    if (!pe_ || !memory_) {
        return;
    }
    quickValues_[0]->setText(pe_->b_IsPE64() ? "PE32+ (64-bit)" : "PE32 (32-bit)");
    quickValues_[1]->setText(QString::fromUtf8(pe_->getArchitectureName().data(), static_cast<int>(pe_->getArchitectureName().size())));
    quickValues_[2]->setText(QString::fromStdString(pe_->getBuildDateTime()));
    quickValues_[3]->setText(QString::fromStdString(memory_->getHashMD5()));
    quickValues_[4]->setText(QString::fromStdString(memory_->getHash256()));
    quickValues_[5]->setText(formatHex(pe_->qw_getBaseAddress()));
    quickValues_[6]->setText(formatHex(pe_->getEntryPointRva()) + " (RVA)");
    quickValues_[7]->setText(subsystemName());
    quickValues_[8]->setText(formatSize(memory_->getBuffer().size()));
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
    if (!pe_ || !memory_) {
        return;
    }
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
    if (!memory_) {
        return;
    }
    // Строки извлекаются один раз при первом открытии вкладки: для больших файлов это долго.
    if (!stringsExtracted_) {
        strings_ = PE::extractStrings(memory_->getBuffer());
        stringsExtracted_ = true;
    }
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
    if (!pe_ || !memory_) {
        return;
    }
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
    if (!pe_) {
        return;
    }
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
    if (!pe_) {
        return;
    }
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
    if (!pe_) {
        return;
    }
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
    if (!pe_) {
        return;
    }
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
        pageDirty_[0] = false;
        pageDirty_[6] = false;
    }
}

void MainWindow::applyTheme() {
    const QString theme = themeCombo_->currentData().toString();
    if (theme == "dark") {
        // Нативные стили Windows (windows11/vista) игнорируют части кастомной палитры
        // (текст кнопок и комбобоксов выходит белым на белом), поэтому для тем с
        // собственной палитрой нужен Fusion.
        qApp->setStyle(QStyleFactory::create("Fusion"));
    } else if (theme == "light") {
        qApp->setStyle(QStyleFactory::create("Fusion"));
    } else {
        qApp->setStyle(QStyleFactory::create(defaultStyle_));
    }
    // Полупрозрачные цвета строим из целевой палитры: одна таблица стилей на все три темы.
    const QPalette targetPalette = theme == "dark" ? applicationPalette(true)
        : theme == "light" ? applicationPalette(false)
        : qApp->style()->standardPalette();
    const QColor ink = targetPalette.color(QPalette::WindowText);
    const QColor accent = targetPalette.color(QPalette::Highlight);
    const auto rgba = [](const QColor& color, const double alpha) {
        return QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha, 0, 'f', 2);
    };
    // ВАЖЕН ПОРЯДОК: setStyleSheet перезаписывает палитру, заданную ранее setPalette,
    // (текст кнопок выцветает до серого), поэтому палитру применяем ПОСЛЕ таблицы стилей.
    // palette(text) в правиле обязательно: со стилевыми таблицами Qt красит текст
    // readOnly QLineEdit цветом плейсхолдера — в светлой теме он выцветает.
    // Цвета кнопок заданы явно: QStyleSheetStyle иначе рисует их текст мимо палитры.
    qApp->setStyleSheet(QString(R"(
        QPushButton {
            background: %4;
            color: %5;
            border: 1px solid %6;
            border-radius: 4px;
            padding: 4px 14px;
        }
        QPushButton:hover { border-color: %7; }
        QPushButton:pressed { background: %8; }
        QPushButton:disabled { color: %9; }
        QToolButton[nav="true"] {
            text-align: left;
            padding: 7px 9px;
            border: 0;
            border-radius: 5px;
            font-family: "Cascadia Mono", Consolas, monospace;
            font-size: %10px;
        }
        QToolButton[nav="true"]:hover { background: %1; }
        QToolButton[nav="true"]:checked {
            background: %2;
            color: palette(highlight);
            border-left: 2px solid palette(highlight);
            font-weight: 600;
        }
        QLabel[pageTitle="true"] {
            font-family: "Cascadia Mono", Consolas, monospace;
            font-weight: 600;
        }
        QLineEdit[readOnly="true"] { background: transparent; border: 0; color: palette(text); }
        QLabel[formLabel="true"] { color: %3; }
    )").arg(rgba(ink, 0.07), rgba(accent, 0.13), rgba(ink, 0.72),
        targetPalette.color(QPalette::Button).name(),
        targetPalette.color(QPalette::ButtonText).name(),
        targetPalette.color(QPalette::Mid).name(),
        accent.name(),
        rgba(ink, 0.07),
        rgba(ink, 0.4))
        .arg(12 + fontDelta_));
    qApp->setPalette(targetPalette);
    // Смена стиля+палитры+QSS на живом приложении применяется частично: часть виджетов
    // продолжает рисоваться данными прошлой темы (кнопка Open… выходила бледной).
    // Каноничный фикс — принудительный re-polish всех виджетов.
    for (QWidget* widget : QApplication::allWidgets()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }
}

void MainWindow::retranslateUi() {
    const char* englishNames[] { "Quick Info", "Sections", "Strings", "Disasm", "Imports", "TLS", "Resources", "HEX", "Packers", "Settings" };
    const char* russianNames[] { "Сводка", "Секции", "Строки", "Дизассемблер", "Импорт", "TLS", "Ресурсы", "HEX", "Упаковщики", "Настройки" };
    for (int i = 0; i < 10; ++i) {
        const QString name = language_ == "ru" ? QString::fromUtf8(russianNames[i]) : QString::fromUtf8(englishNames[i]);
        navButtons_[i]->setText(name);
        pageTitles_[i]->setText(name);
    }
    openButton_->setText(text("Open…", "Открыть…"));

    const char* quickEnglish[] { "Bitness", "Architecture", "Build date", "MD5", "SHA256", "ImageBase", "Entry point", "Subsystem", "File size" };
    const char* quickRussian[] { "Битность", "Архитектура", "Дата сборки", "MD5", "SHA256", "ImageBase", "Точка входа", "Подсистема", "Размер файла" };
    for (int i = 0; i < 9; ++i) {
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
    packerLabels_[0]->setText(text("Packer", "Упаковщик"));
    packerLabels_[1]->setText(text("Xenolith profile", "Профиль Xenolith"));
    packerLabels_[2]->setText(text("Virtualized exports", "Виртуализируемые экспорты"));
    packerLabels_[3]->setText(text("Output", "Результат"));
    xenolithProfileCombo_->setItemText(0, text("Fast", "Быстрый"));
    xenolithProfileCombo_->setItemText(1, text("Standard", "Стандартный"));
    xenolithProfileCombo_->setItemText(2, text("Max", "Максимальный"));
    xenolithExportsEdit_->setPlaceholderText(text("Export names, comma-separated", "Имена экспортов через запятую"));
    packButton_->setText(text("Pack", "Упаковать"));
    // Заметка и состояние кнопки пересобираются под текущий файл и выбранный упаковщик.
    updatePackers();
    hexEditor_->retranslate(language_ == "ru");
    if (pe_) {
        // Переводим содержимое только уже заполненных вкладок — ленивые соберутся при первом открытии.
        if (!pageDirty_[0]) updateQuickInfo();
        if (!pageDirty_[3]) updateDisasm();
        if (!pageDirty_[4]) updateImportFunctions(importsList_->currentRow());
        if (!pageDirty_[5]) updateTls();
    }
    updateStatus();
}

void MainWindow::loadSettings() {
    if (!settingsEnabled()) {
        return;
    }
    const QSettings settings;
    theme_ = settings.value("theme", theme_).toString();
    language_ = settings.value("language", language_).toString();
    units_ = settings.value("units", units_).toString();
    packer_ = settings.value("packer", packer_).toString();
    disasmSpacingSaved_ = settings.value("disasmSpacing", disasmSpacingSaved_).toInt();
    fontDelta_ = std::clamp(settings.value("fontDelta", 0).toInt(), -3, 10);
    lastDir_ = settings.value("lastDir").toString();
    savedGeometry_ = settings.value("geometry").toByteArray();
}

void MainWindow::saveSettings() {
    if (!settingsEnabled()) {
        return;
    }
    QSettings settings;
    settings.setValue("theme", themeCombo_->currentData().toString());
    settings.setValue("language", languageCombo_->currentData().toString());
    settings.setValue("units", unitsCombo_->currentData().toString());
    settings.setValue("packer", packerCombo_->currentData().toString());
    settings.setValue("disasmSpacing", disasmSpacing_->value());
    settings.setValue("fontDelta", fontDelta_);
    if (!filePath_.isEmpty()) {
        settings.setValue("lastDir", QFileInfo(filePath_).absolutePath());
    }
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::openFileDialog() {
    const QString startDir = filePath_.isEmpty() ? lastDir_ : filePath_;
    const QString path = QFileDialog::getOpenFileName(this,
        text("Open PE file", "Открыть PE-файл"),
        startDir,
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

// --- Packers ---

void MainWindow::updatePackers() {
    if (!packerCombo_) {
        return;
    }
    const QString key = packerCombo_->currentData().toString();
    // У Xenolith есть дополнительные опции; UPX вызывается только с -o и входным файлом.
    packersForm_->setRowVisible(1, key == "xenolith");
    packersForm_->setRowVisible(2, key == "xenolith");

    if (!memory_) {
        packerOutputEdit_->clear();
        packerNoteLabel_->setText(text("Open a PE file first.", "Сначала откройте PE-файл."));
        packButton_->setEnabled(false);
        return;
    }
    // Упакованный файл всегда ложится рядом с оригиналом.
    packerOutputEdit_->setText(QDir::toNativeSeparators(defaultPackedPath(key)));
    if (packerExecutable(key).isEmpty()) {
        packerNoteLabel_->setText(text("Packer executable not found in the Packers folder.", "Исполняемый файл упаковщика не найден в папке Packers."));
        packButton_->setEnabled(false);
        return;
    }
    if (key == "xenolith" && !pe_->b_IsPE64()) {
        packerNoteLabel_->setText(text("Xenolith supports only 64-bit (x64) PE files.", "Xenolith поддерживает только 64-битные (x64) PE-файлы."));
        packButton_->setEnabled(false);
        return;
    }
    updatePackerNote();
    packButton_->setEnabled(!packRunning_);
}

void MainWindow::updatePackerNote() {
    const QString key = packerCombo_->currentData().toString();
    if (key == "upx") {
        packerNoteLabel_->setText(text("Classic compression: upx.exe -o <output> <input>. The result can be unpacked with 'upx -d'.", "Классическое сжатие: upx.exe -o <результат> <исходник>. Результат можно распаковать командой 'upx -d'."));
    } else {
        packerNoteLabel_->setText(text("Seals pages with ChaCha20-Poly1305 and compiles the selected exports into per-build unique native code. FastBinary calls its 'pack' command directly on the open file — no console wizard.", "Запечатывает страницы ChaCha20-Poly1305 и компилирует выбранные экспорты в уникальный для каждой сборки машинный код. FastBinary вызывает его команду 'pack' прямо над открытым файлом — консольный мастер не запускается."));
    }
}

void MainWindow::startPack() {
    if (!memory_ || !pe_ || packRunning_) {
        return;
    }
    const QString key = packerCombo_->currentData().toString();
    const QString executable = packerExecutable(key);
    if (executable.isEmpty()) {
        QMessageBox::warning(this, text("Packer not found", "Упаковщик не найден"),
            text("Packer executable not found in the Packers folder.", "Исполняемый файл упаковщика не найден в папке Packers."));
        return;
    }
    if (key == "xenolith" && !pe_->b_IsPE64()) {
        QMessageBox::warning(this, text("Unsupported file", "Файл не поддерживается"),
            text("Xenolith supports only 64-bit (x64) PE files.", "Xenolith поддерживает только 64-битные (x64) PE-файлы."));
        return;
    }

    const QString input = QFileInfo(filePath_).absoluteFilePath();
    packOutputPath_ = QFileInfo(defaultPackedPath(key)).absoluteFilePath();

    QStringList arguments;
    if (key == "xenolith") {
        // Xenolith умеет неинтерактивную сабкоманду pack: TUI с файловым менеджером не нужен.
        arguments << "pack" << input << "-o" << packOutputPath_
                  << "--profile" << xenolithProfileCombo_->currentData().toString();
        // --vm-export принимает имена через запятую; убираем пробелы вокруг них.
        QStringList exports;
        for (QString part : xenolithExportsEdit_->text().split(',', Qt::SkipEmptyParts)) {
            part = part.trimmed();
            if (!part.isEmpty()) {
                exports << part;
            }
        }
        if (!exports.isEmpty()) {
            arguments << "--vm-export" << exports.join(',');
        }
    } else {
        // UPX 5.3.0 не понимает --ultra; держим минимальный проверенный вызов:
        // upx.exe -o <выходной файл> <входной файл>. Уровни сжатия не передаём.
        arguments << "-o" << packOutputPath_ << input;
    }

    if (!packProcess_) {
        packProcess_ = new QProcess(this);
        packProcess_->setProcessChannelMode(QProcess::MergedChannels);
        // Пакеры — консольные утилиты: без CREATE_NO_WINDOW Windows покажет консольное окно.
        packProcess_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* create) {
            create->flags |= CREATE_NO_WINDOW;
        });
        connect(packProcess_, &QProcess::readyReadStandardOutput, this, [this] {
            appendPackChunk(QString::fromUtf8(packProcess_->readAll()));
        });
        connect(packProcess_, &QProcess::finished, this, [this](const int exitCode, const QProcess::ExitStatus status) {
            finishPack(exitCode, status != QProcess::NormalExit);
        });
        connect(packProcess_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                appendPackChunk(text("Failed to start the packer.\n", "Не удалось запустить упаковщик.\n"));
                finishPack(-1, true);
            }
        });
    }

    packerLog_->clear();
    appendPackChunk("> " + QFileInfo(executable).fileName() + " " + arguments.join(" ") + "\n\n");
    packRunning_ = true;
    packButton_->setEnabled(false);
    packProcess_->setWorkingDirectory(QFileInfo(executable).absolutePath());
    packProcess_->start(executable, arguments);
}

void MainWindow::appendPackChunk(const QString& chunk) {
    // Консольные утилиты рисуют прогресс через \r и \b — приводим к обычным строкам.
    QString normalized = chunk;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    normalized.remove('\b');
    packerLog_->moveCursor(QTextCursor::End);
    packerLog_->insertPlainText(normalized);
    packerLog_->moveCursor(QTextCursor::End);
}

void MainWindow::finishPack(const int exitCode, const bool crashed) {
    packRunning_ = false;
    packButton_->setEnabled(memory_ != nullptr && !packerExecutable(packerCombo_->currentData().toString()).isEmpty());
    if (crashed || exitCode != 0) {
        appendPackChunk("\n" + text("Packing failed.", "Ошибка упаковки.") + "\n");
        QMessageBox::warning(this, text("Packing failed", "Ошибка упаковки"),
            crashed ? text("The packer terminated abnormally.", "Упаковщик завершился аварийно.")
                    : text("The packer returned exit code %1.", "Упаковщик завершился с кодом %1.").arg(exitCode));
        return;
    }
    appendPackChunk("\n" + text("Packed successfully.", "Упаковано успешно.") + "\n");
    const QString native = QDir::toNativeSeparators(packOutputPath_);
    const auto answer = QMessageBox::question(this,
        text("Packing complete", "Упаковка завершена"),
        text("The packed file is saved next to the original:", "Упакованный файл сохранён рядом с оригиналом:") + "\n" + native + "\n\n"
            + text("Load it for analysis?", "Открыть его для анализа?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        loadFile(packOutputPath_);
    }
}

QString MainWindow::packerExecutable(const QString& key) const {
    const QString name = key == "upx" ? "upx.exe" : "xenolith.exe";
    QStringList candidates;
    // Переопределение каталога пакеров — для тестов и портабельных сборок.
    const QString overrideDir = qEnvironmentVariable("FB_PACKERS_DIR");
    if (!overrideDir.isEmpty()) {
        candidates << overrideDir + "/" + name;
    }
    // Рядом с exe, а затем вверх по дереву каталогов (cmake-build-debug, out/, ...).
    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << appDir + "/Packers/" + name
               << appDir + "/../Packers/" + name
               << appDir + "/../../Packers/" + name
               << appDir + "/../../../Packers/" + name
               << appDir + "/../../../../Packers/" + name;
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return QString();
}

QString MainWindow::defaultPackedPath(const QString& key) const {
    const QFileInfo info(filePath_);
    const QString extension = info.suffix().isEmpty() ? QString() : "." + info.suffix();
    const QString marker = key == "xenolith" ? ".xl" : key == "upx" ? "_upx" : "_packed";
    return info.absolutePath() + "/" + info.completeBaseName() + marker + extension;
}

// --- Размер шрифта ---

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            auto* widget = qobject_cast<QWidget*>(watched);
            if (widget && (widget == this || isAncestorOf(widget))) {
                changeFontSize(wheel->angleDelta().y() > 0 ? 1 : -1);
                return true; // не прокручиваем таблицу/лог одновременно с изменением шрифта
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeFontSize(const int delta) {
    const int next = std::clamp(fontDelta_ + delta, -3, 10);
    if (next == fontDelta_) {
        return;
    }
    fontDelta_ = next;
    applyFonts();
}

void MainWindow::applyFonts() {
    // Шрифт меняется, а не масштаб: layout и размеры виджетов остаются прежними,
    // увеличивается только pointSize/pixelSize у уже установленных шрифтов.
    const auto scaled = [](const QFont& base, const int delta) {
        QFont font = base;
        if (font.pointSize() > 0) {
            font.setPointSize(std::max(6, font.pointSize() + delta));
        } else if (font.pixelSize() > 0) {
            font.setPixelSize(std::max(8, font.pixelSize() + delta * 2));
        }
        return font;
    };

    if (!baseFontsCaptured_) {
        baseAppFont_ = qApp->font();
        // Фиксируем исходные шрифты один раз, чтобы повторные Ctrl++ не складывались.
        for (QWidget* widget : findChildren<QWidget*>()) {
            widget->setProperty("fbBaseFont", QVariant::fromValue(widget->font()));
        }
        baseFontsCaptured_ = true;
    }

    qApp->setFont(scaled(baseAppFont_, fontDelta_));
    // У навигационных кнопок размер задан в QSS — сначала пересобираем таблицу стилей,
    // иначе re-polish сбросил бы только что установленные шрифты виджетов.
    applyTheme();
    for (QWidget* widget : findChildren<QWidget*>()) {
        const QVariant base = widget->property("fbBaseFont");
        if (base.isValid()) {
            widget->setFont(scaled(base.value<QFont>(), fontDelta_));
        }
    }
    if (hexEditor_) {
        hexEditor_->setRowHeightForFont(fontDelta_);
    }
}
