#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once
#include <QMainWindow>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class ConfigManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // UIのテーブルにコメントを追加する
    void addCommentToView(const QString& username, const QString& message);
    void setConfigManager(ConfigManager* configManager);

signals:
    // 設定タブで認証ボタンが押されたことを通知
    void authRequested();
    void bouyomiTestRequested(const QString& message);

private slots:
    void on_btnStartAuth_clicked();
    void on_btnBrowseBouyomi_clicked();
    void on_btnSaveBouyomi_clicked();
    void on_btnTestBouyomi_clicked();
    void on_btnSaveObs_clicked();
    void on_btnCopyObsUrl_clicked();
    void on_btnOpenOverlayFolder_clicked();
    
    void on_btnToggleBouyomi_toggled(bool checked);
    void on_btnToggleObs_toggled(bool checked);

    void on_btnReadme_clicked();
    void on_btnAbout_clicked();

public slots:
    void appendAnalysisLog(const QString& logMsg);

private:
    void loadOverlayFiles();

    Ui::MainWindow *ui;
    QStandardItemModel* m_chatModel;
    ConfigManager* m_configManager = nullptr;
};
#endif // MAINWINDOW_H
