#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    a.setOrganizationName("USOGUII");
    a.setOrganizationDomain("STANKIN");
    a.setApplicationName("RatingSystemSimulation");
    a.setApplicationVersion("0.0.1");
    return a.exec();
}
