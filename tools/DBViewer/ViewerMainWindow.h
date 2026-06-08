#pragma once

#include <QMainWindow>
#include <QStandardItemModel>

namespace Ui {
class ViewerMainWindow;
}

class ViewerMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ViewerMainWindow(const QString& dbPath = "", QWidget *parent = nullptr);
    ~ViewerMainWindow();

private slots:
    void on_btnReload_clicked();
    void on_btnDeleteSelected_clicked();
    void on_btnDeleteAll_clicked();

private:
    void loadDatabase();

    Ui::ViewerMainWindow *ui;
    QStandardItemModel *m_model;
    QString m_dbPath;
};
