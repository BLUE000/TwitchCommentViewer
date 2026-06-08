#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once
#include <QMainWindow>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // UIのテーブルにコメントを追加する
    void addCommentToView(const QString& username, const QString& message);

signals:
    // 設定タブで認証ボタンが押されたことを通知
    void authRequested();

private slots:
    void on_btnStartAuth_clicked();

private:
    Ui::MainWindow *ui;
    QStandardItemModel* m_chatModel;
};
#endif // MAINWINDOW_H
