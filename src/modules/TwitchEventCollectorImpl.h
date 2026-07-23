#pragma once
#include "ITwitchEventCollector.h"
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>

#include <QHash>

#include <QTimer>
#include <QUrl>

class TwitchEventCollectorImpl : public QObject, public ITwitchEventCollector {
    Q_OBJECT
public:
    explicit TwitchEventCollectorImpl(QObject* eventTarget, QObject* parent = nullptr);
    ~TwitchEventCollectorImpl() override;

    void connectToTwitch() override;
    void disconnectFromTwitch() override;
    void requestChatters() override;
    void sendShoutout(const QString& toBroadcasterId) override;
    void setVipStatus(const QString& targetUserId, bool enable) override;
    void setModeratorStatus(const QString& targetUserId, bool enable) override;
    void banUser(const QString& targetUserId, int duration, const QString& reason = "") override;
    void unbanUser(const QString& targetUserId) override;
    void pinChatMessage(const QString& messageId, int durationSeconds) override;
    void unpinChatMessage(const QString& messageId) override;
    void sendAnnouncement(const QString& message, const QString& color) override;

    // 認証情報をセットする
    void setAuthData(const QString& clientId, const QString& accessToken);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onKeepaliveTimeout();
    void attemptReconnect();

private:
    QWebSocket m_webSocket;
    QObject* m_eventTarget; // イベント送信先（AppController等のハンドラ）
    
    QString m_clientId;
    QString m_accessToken;
    QString m_sessionId;
    QString m_broadcasterId; // TwitchのユーザーID

    QHash<QString, QString> m_badgeUrls; // set_id:version -> image_url

    QTimer m_reconnectTimer;
    QTimer m_keepaliveTimer;
    int m_reconnectIntervalMs{1000};
    bool m_isIntentionalDisconnect{false};
    bool m_isReconnecting{false};

    void processNotification(const QJsonObject& json);
    void handleSessionReconnect(const QJsonObject& json);
    void resetKeepaliveTimer();
    void scheduleReconnect();
    void fetchBroadcasterIdAndSubscribe();
    void registerSubscription();
    void sendSubscriptionRequest(const QString& type, const QString& version, const QJsonObject& condition);
    void checkCurrentStreamStatus();
    void fetchBadges();
    void parseAndCacheBadges(const QJsonDocument& doc);
};
