#pragma once
#include <QObject>
#include <QEvent>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include "interfaces/ITwitchEventCollector.h"
#include "interfaces/IBouyomiIntegration.h"
#include "interfaces/ICommentAnalyzer.h"
#include "interfaces/IObsIntegration.h"
#include "modules/ObsWebSocketServer.h"
#include "modules/ObsHttpServer.h"
#include "events/TwitchEvents.h"
#include "modules/DatabaseManager.h"
#include "modules/ConfigManager.h"

class MainWindow;

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void initialize();
    void setMainWindow(MainWindow* mw) { m_mainWindow = mw; }
    void updateBotUsers(const QStringList& bots);

protected:
    // シグナルの代わりにEvent割り込みで処理を受け取る
    void customEvent(QEvent* event) override;

private:
    MainWindow* m_mainWindow = nullptr;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<ITwitchEventCollector> m_twitchCollector;
    std::unique_ptr<IBouyomiIntegration> m_bouyomiIntegration;
    std::unique_ptr<IObsIntegration> m_obsFileIntegration;
    std::unique_ptr<ObsWebSocketServer> m_obsWsIntegration;
    std::unique_ptr<ObsHttpServer> m_obsHttpServer;
    std::unique_ptr<ICommentAnalyzer> m_commentAnalyzer;
    std::unique_ptr<DatabaseManager> m_dbManager;

    // 視聴者リスト制御用メンバ
    QDateTime m_lastChattersFetchTime;
    QTimer* m_chattersTimer = nullptr;
    bool m_chattersTabActive = false;

    void onTabWidgetChanged(int index);
    void onChattersTimerTimeout();
    void triggerChattersFetch();
    
    // UI通知用の中継メソッド（解析タブ用）
    void emitSpamDetected(const QString& username, const QString& reason, const QString& message);
    void emitTrendWordDetected(const QString& word, int count);
    void emitEmotionScored(const QString& username, const QString& message, double score);
    void emitStatisticsUpdated(int totalComments, const QMap<QString, int>& userCounts, const QString& latestUser, const QDateTime& latestTime);
};
