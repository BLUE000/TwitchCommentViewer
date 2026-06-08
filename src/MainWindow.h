#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#pragma once
#include <QMainWindow>
#include <QStandardItemModel>
#include <QDateTime>
#include <QTimer>
#include <QMap>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class ConfigManager;
class DatabaseManager;
struct CommentLog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // UIのテーブルにコメントを追加する
    void addCommentToView(const QString& username, const QString& message);
    void setConfigManager(ConfigManager* configManager);
    void setDatabaseManager(DatabaseManager* dbManager);
    void loadHistoryFromDb();

signals:
    // 設定タブで認証ボタンが押されたことを通知
    void authRequested();
    void bouyomiTestRequested(const QString& message);
    void botSettingsChanged(const QStringList& bots);

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
    void on_btnToggleBot_toggled(bool checked);
    void on_btnToggleDb_toggled(bool checked);

    void on_btnSaveBot_clicked();

    void on_btnReadme_clicked();
    void on_btnAbout_clicked();

public slots:
    void appendAnalysisLog(const QString& logMsg);
    void updateStatistics(int totalComments, const QMap<QString, int>& userCounts, const QString& latestUser = "", const QDateTime& latestTime = QDateTime());

private slots:
    void onChartConfigChanged();
    void on_btnLaunchDbViewer_clicked();
    void updateChartDisplay();

private:
    void loadOverlayFiles();
    void setupChart();

    Ui::MainWindow *ui;
    QStandardItemModel* m_chatModel;
    ConfigManager* m_configManager = nullptr;
    DatabaseManager* m_dbManager = nullptr;

    // グラフ関連
    QChart* m_chart = nullptr;
    QDateTimeAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;
    QTimer* m_chartUpdateTimer = nullptr;

    // 履歴データ
    struct LogEntry {
        QDateTime time;
        QString user;
    };
    QList<LogEntry> m_commentHistory;
    bool m_chartNeedsUpdate = false;
};
#endif // MAINWINDOW_H
