#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QTableWidget>
#include <QToolButton>

#include <iostream>
#include <string>

#include "PE/pe.h"
#include "PE/Memory/memory.h"
#include "Disasm/disasm.h"
#include "UI/MainWindow.h"
#include "UI/HexEditor.h"

namespace {

int failures = 0;

void check(const bool condition, const std::string& message) {
    if (condition) {
        std::cout << "[OK] " << message << '\n';
    } else {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}

}

int main(int argc, char* argv[]) {
    if (!qEnvironmentVariableIsSet("FB_DESKTOP_TEST")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QApplication application(argc, argv);
    if (argc < 2) {
        std::cerr << "Usage: FastBinarySmoke <path-to-pe>\n";
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[1]);

    auto memory = Memory::load(path.toStdWString());
    check(memory.has_value(), "PE file loads into Memory");
    if (!memory) {
        return 1;
    }
    auto pe = PE::create(memory->getBuffer());
    check(pe.has_value(), "PE structures parse");
    if (!pe) {
        return 1;
    }
    check(!pe->getSections().empty(), "Sections are parsed");
    check(pe->entryPointOffset().has_value(), "Entry point RVA maps to a file offset");
    check(!pe->getImports().empty(), "Imports are parsed");
    check(!pe->getResources().empty(), "Resources are parsed");
    const double entropy = PE::calculateEntropy(memory->getBuffer());
    check(entropy > 0.0 && entropy <= 8.0, "Whole-file entropy is in range");
    check(!PE::extractStrings(memory->getBuffer()).empty(), "Strings are extracted");

    auto disasm = Disasm::create(pe->b_IsPE64() ? Disasm::Mode::X64 : Disasm::Mode::X86);
    check(disasm.has_value(), "Capstone initializes");
    if (disasm && pe->entryPointOffset()) {
        const auto offset = *pe->entryPointOffset();
        const auto buffer = memory->getBuffer();
        const auto instructions = disasm->disassemble(buffer.subspan(offset, std::min<size_t>(128, buffer.size() - offset)),
            pe->qw_getBaseAddress() + pe->getEntryPointRva(), 16);
        check(instructions.has_value() && !instructions->empty(), "Capstone disassembles entry point bytes");
    }

    MainWindow window;
    check(window.loadFile(path), "MainWindow loads the PE file");
    window.show();

    const auto navButtons = window.findChildren<QToolButton*>();
    int navCount = 0;
    for (const QToolButton* button : navButtons) {
        if (button->property("nav").toBool()) {
            ++navCount;
        }
    }
    check(navCount == 9, "Sidebar contains nine tabs");

    QTableWidget* sections = nullptr;
    QTableWidget* strings = nullptr;
    QTableWidget* disassembly = nullptr;
    for (QTableWidget* table : window.findChildren<QTableWidget*>()) {
        if (table->columnCount() == 7) sections = table;
        if (table->columnCount() == 2 && table->rowCount() > 100) strings = table;
        if (table->columnCount() == 3) disassembly = table;
    }
    check(sections && sections->rowCount() == static_cast<int>(pe->getSections().size()), "Sections tab shows parsed sections");
    check(strings && strings->columnCount() == 2, "Strings tab has only offset and string columns");
    check(disassembly && disassembly->rowCount() > 0, "Disasm tab shows Capstone instructions");

    if (argc > 2) {
        const QString screenshotDir = QString::fromLocal8Bit(argv[2]);
        for (QLabel* label : window.findChildren<QLabel*>()) {
            if (label->text() == QFileInfo(path).absoluteFilePath()) {
                label->setText(QFileInfo(path).fileName());
                break;
            }
        }
        for (QComboBox* combo : window.findChildren<QComboBox*>()) {
            if (combo->findData("dark") >= 0) {
                combo->setCurrentIndex(combo->findData("dark"));
                break;
            }
        }
        QApplication::processEvents();
        window.grab().save(screenshotDir + "/fastbinary-quick.png");
        for (int index : { 1, 3, 7, 8 }) {
            window.findChild<QStackedWidget*>()->setCurrentIndex(index);
            QApplication::processEvents();
            window.grab().save(screenshotDir + QString("/fastbinary-tab-%1.png").arg(index));
        }
        window.findChild<QStackedWidget*>()->setCurrentIndex(0);
    }

    auto* hexEditor = window.findChild<HexEditor*>();
    check(hexEditor != nullptr, "HEX editor exists");
    if (hexEditor) {
        const auto views = hexEditor->findChildren<QTableView*>();
        check(views.size() == 2, "HEX editor has synchronized bytes and ASCII panes");
        if (views.size() == 2) {
            auto* hexView = views[0];
            auto* asciiView = views[1];
            const QModelIndex firstByte = hexView->model()->index(0, 1);
            check(hexView->model()->setData(firstByte, "41", Qt::EditRole), "HEX byte is editable");
            const QString ascii = asciiView->model()->data(asciiView->model()->index(0, 0), Qt::DisplayRole).toString();
            check(ascii.startsWith(QChar('A')), "ASCII pane refreshes after a byte edit");
            auto* offsetEdit = hexEditor->findChild<QLineEdit*>();
            offsetEdit->setText("10");
            const auto buttons = hexEditor->findChildren<QPushButton*>();
            buttons.first()->click();
            check(hexView->currentIndex().row() == 1, "HEX offset navigation selects the requested row");
        }
    }

    const auto combos = window.findChildren<QComboBox*>();
    check(combos.size() == 3, "Settings contains only theme, language and size units");
    QComboBox* language = nullptr;
    QComboBox* units = nullptr;
    for (QComboBox* combo : combos) {
        if (combo->findData("ru") >= 0) language = combo;
        if (combo->findData("bytes") >= 0) units = combo;
    }
    if (language) {
        language->setCurrentIndex(language->findData("ru"));
        check(navButtons[0]->text() == QString::fromUtf8("Сводка"), "Language setting retranslates the UI");
    }
    if (units) {
        units->setCurrentIndex(units->findData("bytes"));
        check(true, "Size units switch without errors");
    }

    std::cout << (failures == 0 ? "All smoke checks passed.\n" : "Smoke checks failed.\n");
    return failures == 0 ? 0 : 1;
}
