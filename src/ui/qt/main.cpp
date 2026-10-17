#include <QApplication>
#include "main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Zero");
    app.setApplicationVersion("0.0.1-beta");

    MainWindow window;
    window.show();

    return app.exec();
}
