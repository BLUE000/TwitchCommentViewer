#include <QApplication>
#include <QStringList>
#include "ViewerMainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    QString dbPath = "";
    if (a.arguments().size() > 1) {
        dbPath = a.arguments().at(1);
    }
    
    ViewerMainWindow w(dbPath);
    w.show();
    
    return a.exec();
}
