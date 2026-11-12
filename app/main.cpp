#include "src/ui/MainWindow/MainWindow.h"
#include "src/pages/DashboardPage.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    // DashboardPage w;
    // w.show();
    return QApplication::exec();
}
