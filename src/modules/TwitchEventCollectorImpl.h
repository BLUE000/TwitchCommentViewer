#pragma once
#include "ITwitchEventCollector.h"
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>

#include <QHash>

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

    // 認証情報をセットする
    void setAuthData(const QString& clientId, const QString& accessToken);

private slots:
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QWebSocket m_webSocket;
    QObject* m_eventTarget; // イベント送信先（AppController等のハンドラ）
    
    QString m_clientId;
    QString m_accessToken;
    QString m_sessionId;
    QString m_broadcasterId; // TwitchのユーザーID

    QHash<QString, QString> m_badgeUrls; // set_id:version -> image_url

    void processNotification(const QJsonObject& json);
    void fetchBroadcasterIdAndSubscribe();
    void registerSubscription();
    void fetchBadges();
    void parseAndCacheBadges(const QJsonDocument& doc);
};
