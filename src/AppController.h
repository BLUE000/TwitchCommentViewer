#pragma once
#include <QObject>
#include <QEvent>
#include <QDateTime>
#include <QTimer>
#include <memory>
#include "interfaces/ITwitchEventCollector.h"
#include "interfaces/ITtsIntegration.h"
#include "interfaces/ICommentAnalyzer.h"
#include "interfaces/IObsIntegration.h"
#include "modules/ObsWebSocketServer.h"
#include "modules/ObsHttpServer.h"
#include "events/TwitchEvents.h"
#include "modules/DatabaseManager.h"
#include "modules/ConfigManager.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QIcon>
#include <QHash>
#include <QSet>

class MainWindow;

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

public slots:
    void initialize();
    void setMainWindow(MainWindow* mw) { m_mainWindow = mw; }
    void updateBotUsers(const QStringList& bots);
    
    // UIからの要求を処理するスロット
    void startOAuthFlow();
    void ttsTestRequested(const QString& message);
    void onTabWidgetChanged(int index);
    void onChattersTimerTimeout();
    void triggerChattersFetch();

    void onChatterShoutoutRequested(const QString& targetUserId);
    void onChatterVipToggled(const QString& targetUserId, bool enable);
    void onChatterModeratorToggled(const QString& targetUserId, bool enable);
    void onChatterTimeoutRequested(const QString& targetUserId, int duration);
    void onChatterBanToggled(const QString& targetUserId, bool enable);
    void onPinCommentRequested(const QString& messageId, int durationSeconds);
    void onUnpinCommentRequested(const QString& messageId);
    void onSendAnnouncementRequested(const QString& message, const QString& color);

protected:
    // シグナルの代わりにEvent割り込みで処理を受け取る
    void customEvent(QEvent* event) override;

private:
    MainWindow* m_mainWindow = nullptr;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<ITwitchEventCollector> m_twitchCollector;
    std::unique_ptr<ITtsIntegration> m_ttsIntegration;
    std::unique_ptr<IObsIntegration> m_obsFileIntegration;
    std::unique_ptr<ObsWebSocketServer> m_obsWsIntegration;
    std::unique_ptr<ObsHttpServer> m_obsHttpServer;
    std::unique_ptr<ICommentAnalyzer> m_commentAnalyzer;
    std::unique_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<QThread> m_dbThread;
    std::unique_ptr<QThread> m_analyzerThread;

    // 視聴者リスト制御用メンバ
    QDateTime m_lastChattersFetchTime;
    QTimer* m_chattersTimer = nullptr;
    bool m_chattersTabActive = false;


    
    // UI通知用の中継メソッド（解析タブ用）
    void emitSpamDetected(const QString& username, const QString& reason, const QString& message);
    void emitTrendWordDetected(const QString& word, int count);
    void emitEmotionScored(const QString& username, const QString& message, double score);
    void emitStatisticsUpdated(int totalComments, const QMap<QString, int>& userCounts, const QString& latestUser, const QDateTime& latestTime);

    // アバター取得用
    std::unique_ptr<QNetworkAccessManager> m_nam;
    QHash<QString, QIcon> m_avatarCache;
    QHash<QString, QString> m_avatarUrlCache;
    QSet<QString> m_pendingAvatars;

    void fetchAvatar(const QString& userId);
    void downloadAvatarImage(const QString& userId, const QString& urlStr);
};
