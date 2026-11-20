#pragma once

#include <QFont>
#include <QMainWindow>
#include <memory>
#include <vector>

#include "PE/pe.h"
#include "PE/Memory/memory.h"

class QComboBox;
class QFormLayout;
class QTableWidget;
class QLineEdit;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProcess;
class QProgressBar;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;
class HexEditor;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool loadFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    QWidget* createPage(int index);
    void loadSettings();
    void saveSettings();
    void updatePage(int index);
    void updateCurrentPage();
    void markPagesDirty();
    void updateQuickInfo();
    void updateSections();
    void updateStrings();
    void filterStrings();
    void updateDisasm();
    void updateImports();
    void updateImportFunctions(int row);
    void updateTls();
    void updateResources();
    void updateStatus();
    void retranslateUi();
    void applyTheme();
    void applyUnits();
    void openFileDialog();

    // Размер шрифта (не масштаб): Ctrl+колёсико / Ctrl+± / Ctrl+0.
    void changeFontSize(int delta);
    void applyFonts();

    // Вкладка Packers: сборка состояния, запуск выбранного упаковщика и разбор результата.
    void updatePackers();
    void updatePackerNote();
    void startPack();
    void appendPackChunk(const QString& chunk);
    void finishPack(int exitCode, bool crashed);
    [[nodiscard]] QString packerExecutable(const QString& key) const;
    [[nodiscard]] QString defaultPackedPath(const QString& key) const;

    [[nodiscard]] QString text(const char* english, const char* russian) const;
    [[nodiscard]] QString formatSize(qulonglong bytes) const;
    [[nodiscard]] QString formatHex(qulonglong value, int digits = 0) const;
    [[nodiscard]] QString subsystemName() const;

    std::unique_ptr<Memory> memory_;
    std::unique_ptr<PE> pe_;
    std::vector<PE::StringEntry> strings_;
    QString filePath_;
    QString language_ { "en" };
    QString units_ { "auto" };
    QString theme_ { "auto" };
    QString packer_ { "xenolith" };
    QString defaultStyle_;
    QString lastDir_;
    QByteArray savedGeometry_;
    int disasmSpacingSaved_ { 24 };
    int fontDelta_ { 0 };
    QFont baseAppFont_;
    bool baseFontsCaptured_ { false };
    bool stringsExtracted_ { false };
    bool pageDirty_[10] {};

    QToolButton* navButtons_[10] {};
    QLabel* pageTitles_[10] {};
    QStackedWidget* stack_ { nullptr };
    QPushButton* openButton_ { nullptr };
    QLabel* fileLabel_ { nullptr };
    QLabel* statusLabel_ { nullptr };

    QLabel* quickLabels_[9] {};
    QLineEdit* quickValues_[9] {};

    QLabel* totalEntropyLabel_ { nullptr };
    QLineEdit* totalEntropyValue_ { nullptr };
    QProgressBar* totalEntropyBar_ { nullptr };
    QTableWidget* sectionsTable_ { nullptr };

    QLineEdit* stringsFilter_ { nullptr };
    QLabel* stringsCount_ { nullptr };
    QTableWidget* stringsTable_ { nullptr };

    QLabel* disasmEntryLabel_ { nullptr };
    QLabel* disasmSpacingLabel_ { nullptr };
    QLabel* disasmSpacingValue_ { nullptr };
    QLabel* disasmStatus_ { nullptr };
    QSlider* disasmSpacing_ { nullptr };
    QTableWidget* disasmTable_ { nullptr };

    QListWidget* importsList_ { nullptr };
    QLabel* importsCount_ { nullptr };
    QTableWidget* importsTable_ { nullptr };

    QWidget* tlsContent_ { nullptr };
    QLabel* tlsEmpty_ { nullptr };
    QLabel* tlsLabels_[6] {};
    QLineEdit* tlsValues_[6] {};
    QLabel* tlsCallbacksLabel_ { nullptr };
    QListWidget* tlsCallbacks_ { nullptr };

    QTableWidget* resourcesTable_ { nullptr };

    QLabel* themeLabel_ { nullptr };
    QLabel* languageLabel_ { nullptr };
    QLabel* unitsLabel_ { nullptr };
    QComboBox* themeCombo_ { nullptr };
    QComboBox* languageCombo_ { nullptr };
    QComboBox* unitsCombo_ { nullptr };

    // Вкладка Packers.
    QFormLayout* packersForm_ { nullptr };
    QLabel* packerLabels_[4] {};
    QComboBox* packerCombo_ { nullptr };
    QComboBox* xenolithProfileCombo_ { nullptr };
    QLineEdit* xenolithExportsEdit_ { nullptr };
    QLineEdit* packerOutputEdit_ { nullptr };
    QPushButton* packButton_ { nullptr };
    QLabel* packerNoteLabel_ { nullptr };
    QPlainTextEdit* packerLog_ { nullptr };
    QProcess* packProcess_ { nullptr };
    QString packOutputPath_;
    bool packRunning_ { false };

    HexEditor* hexEditor_ { nullptr };
};
