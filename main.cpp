#include <QApplication>

#include "UI/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("FastBinary");
    QApplication::setOrganizationName("FastBinary");

    MainWindow window;
    window.show();
    if (argc > 1) {
        window.loadFile(QString::fromLocal8Bit(argv[1]));
    }
    return QApplication::exec();
}
