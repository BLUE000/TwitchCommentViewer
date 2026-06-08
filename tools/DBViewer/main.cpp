#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    QMainWindow w;
    w.setWindowTitle("DB Viewer (Mock)");
    QLabel* label = new QLabel("DB Viewer App Started", &w);
    w.setCentralWidget(label);
    
    w.show();
    return a.exec();
}
