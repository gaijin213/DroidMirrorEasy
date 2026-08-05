#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("DroidMirror");
    QApplication::setOrganizationName("DroidMirror");
    QApplication::setApplicationVersion("1.0");

    MainWindow w;
    w.show();
    return app.exec();
}
