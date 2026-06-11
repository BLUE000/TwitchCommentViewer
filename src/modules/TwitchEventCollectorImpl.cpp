#include "TwitchEventCollectorImpl.h"
#include "../events/TwitchEvents.h"
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonArray>

TwitchEventCollectorImpl::TwitchEventCollectorImpl(QObject* eventTarget, QObject* parent)
    : QObject(parent), m_eventTarget(eventTarget)
{
    connect(&m_webSocket, &QWebSocket::connected, this, &TwitchEventCollectorImpl::onConnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &TwitchEventCollectorImpl::onTextMessageReceived);
    // WebSocketのエラー処理
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &TwitchEventCollectorImpl::onErrorOccurred);
}

TwitchEventCollectorImpl::~TwitchEventCollectorImpl() {
    disconnectFromTwitch();
}

void TwitchEventCollectorImpl::connectToTwitch() {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty()) {
        qWarning() << "Cannot connect to Twitch EventSub: Missing Auth Data.";
        return;
    }
    // Twitch EventSub WebSockets の接続先URL
    QUrl url("wss://eventsub.wss.twitch.tv/ws");
    qInfo() << "Connecting to Twitch EventSub (WebSocket)...";
    m_webSocket.open(url);
}

void TwitchEventCollectorImpl::disconnectFromTwitch() {
    if (m_webSocket.isValid()) {
        m_webSocket.close();
        qInfo() << "Disconnected from Twitch EventSub.";
    }
}

void TwitchEventCollectorImpl::setAuthData(const QString& clientId, const QString& accessToken) {
    m_clientId = clientId;
    m_accessToken = accessToken;
}

void TwitchEventCollectorImpl::onConnected() {
    qInfo() << "Connected to Twitch EventSub. Waiting for session_welcome...";
    // ここで Session Welcome メッセージを受け取り、
    // セッションIDを使ってTwitch API(HTTP)にSubscription登録を行う必要があります。
}

void TwitchEventCollectorImpl::onTextMessageReceived(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject json = doc.object();
    QJsonObject metadata = json["metadata"].toObject();
    QString messageType = metadata["message_type"].toString();

    if (messageType == "session_welcome") {
        QJsonObject payload = json["payload"].toObject();
        m_sessionId = payload["session"].toObject()["id"].toString();
        qInfo() << "Session Welcome received from Twitch. Session ID:" << m_sessionId;
        
        // WebSocket接続が確立されたので、APIを使って購読(Subscription)をリクエストする
        fetchBroadcasterIdAndSubscribe();
    } else if (messageType == "notification") {
        processNotification(json);
    } else if (messageType == "session_keepalive") {
        // qInfo() << "Keepalive received.";
    }
}

void TwitchEventCollectorImpl::processNotification(const QJsonObject& json) {
    QJsonObject payload = json["payload"].toObject();
    QJsonObject eventObj = payload["event"].toObject();
    QJsonObject subscription = payload["subscription"].toObject();

    QString subType = subscription["type"].toString();
    
    // コメントメッセージの場合
    if (subType == "channel.chat.message") {
        QString userId = eventObj["chatter_user_id"].toString();
        QString userName = eventObj["chatter_user_name"].toString();
        QJsonObject messageObj = eventObj["message"].toObject();
        QString text = messageObj["text"].toString();

        // 【要件定義対応】: シグナルを使わず、Event割り込み(postEvent)でターゲットに非同期送信
        if (m_eventTarget) {
            auto* evt = new TwitchEvents::CommentEvent(userId, userName, text);
            QCoreApplication::postEvent(m_eventTarget, evt);
            qDebug() << "Dispatched CommentEvent for user:" << userName;
        }
    }
}

void TwitchEventCollectorImpl::onErrorOccurred(QAbstractSocket::SocketError error) {
    qWarning() << "WebSocket error:" << error << "-" << m_webSocket.errorString();
}

void TwitchEventCollectorImpl::fetchBroadcasterIdAndSubscribe() {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.twitch.tv/helix/users"));
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());

    QNetworkReply* reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray dataObj = doc.object()["data"].toArray();
            if (!dataObj.isEmpty()) {
                m_broadcasterId = dataObj[0].toObject()["id"].toString();
                qInfo() << "Fetched Broadcaster ID:" << m_broadcasterId;
                registerSubscription();
            } else {
                qWarning() << "No user data found.";
            }
        } else {
            qWarning() << "Failed to fetch user data:" << reply->errorString() << reply->readAll();
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::registerSubscription() {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.twitch.tv/helix/eventsub/subscriptions"));
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject condition;
    condition["broadcaster_user_id"] = m_broadcasterId;
    condition["user_id"] = m_broadcasterId; // For reading chat, both are the broadcaster's ID

    QJsonObject transport;
    transport["method"] = "websocket";
    transport["session_id"] = m_sessionId;

    QJsonObject payload;
    payload["type"] = "channel.chat.message";
    payload["version"] = "1";
    payload["condition"] = condition;
    payload["transport"] = transport;

    QJsonDocument doc(payload);
    QNetworkReply* reply = nam->post(request, doc.toJson());
    
    connect(reply, &QNetworkReply::finished, this, [reply, nam]() {
        if (reply->error() == QNetworkReply::NoError) {
            qInfo() << "Successfully subscribed to channel.chat.message via WebSocket!";
        } else {
            qWarning() << "Subscription failed:" << reply->errorString() << reply->readAll();
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::requestChatters() {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot fetch chatters: Missing Auth Data or Broadcaster ID.";
        return;
    }

    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/chat/chatters?broadcaster_id=%1&moderator_id=%2&first=1000")
             .arg(m_broadcasterId).arg(m_broadcasterId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());

    qInfo() << "Requesting chatters list from Twitch Helix API...";
    QNetworkReply* reply = nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray dataArray = doc.object()["data"].toArray();
            QList<TwitchEvents::ChatterInfo> chatterList;
            
            for (const QJsonValue& val : dataArray) {
                QJsonObject obj = val.toObject();
                QString userId = obj["user_id"].toString();
                QString userName = obj["user_name"].toString();
                if (userName.isEmpty()) {
                    userName = obj["user_login"].toString();
                }
                QString userBadge = obj["user_badge"].toString();
                
                TwitchEvents::ChatterInfo info;
                info.userId = userId;
                info.userName = userName;
                info.userBadge = userBadge;
                chatterList.append(info);
            }
            
            qInfo() << "Successfully fetched" << chatterList.size() << "chatters!";
            if (m_eventTarget) {
                auto* evt = new TwitchEvents::ChattersEvent(chatterList);
                QCoreApplication::postEvent(m_eventTarget, evt);
            }
        } else {
            qWarning() << "Failed to fetch chatters:" << reply->errorString() << reply->readAll();
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::sendShoutout(const QString& toBroadcasterId) {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot send shoutout: Missing Auth Data or Broadcaster ID.";
        return;
    }
    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/chat/shoutouts?broadcaster_id=%1&moderator_id=%2&to_broadcaster_id=%3")
             .arg(m_broadcasterId).arg(m_broadcasterId).arg(toBroadcasterId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    
    qInfo() << "Sending shoutout to broadcaster:" << toBroadcasterId;
    QNetworkReply* reply = nam->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [reply, nam]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to send shoutout:" << reply->errorString() << reply->readAll();
        } else {
            qInfo() << "Shoutout sent successfully!";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::setVipStatus(const QString& targetUserId, bool enable) {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot set VIP status: Missing Auth Data or Broadcaster ID.";
        return;
    }
    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/channels/vips?broadcaster_id=%1&user_id=%2")
             .arg(m_broadcasterId).arg(targetUserId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    
    QNetworkReply* reply = nullptr;
    if (enable) {
        qInfo() << "Adding VIP status for user:" << targetUserId;
        reply = nam->post(request, QByteArray());
    } else {
        qInfo() << "Removing VIP status for user:" << targetUserId;
        reply = nam->deleteResource(request);
    }
    
    connect(reply, &QNetworkReply::finished, this, [reply, nam, enable]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to change VIP status (enable:" << enable << "):" << reply->errorString() << reply->readAll();
        } else {
            qInfo() << "VIP status updated successfully (enable:" << enable << ")!";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::setModeratorStatus(const QString& targetUserId, bool enable) {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot set Moderator status: Missing Auth Data or Broadcaster ID.";
        return;
    }
    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/moderation/moderators?broadcaster_id=%1&user_id=%2")
             .arg(m_broadcasterId).arg(targetUserId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    
    QNetworkReply* reply = nullptr;
    if (enable) {
        qInfo() << "Adding Moderator status for user:" << targetUserId;
        reply = nam->post(request, QByteArray());
    } else {
        qInfo() << "Removing Moderator status for user:" << targetUserId;
        reply = nam->deleteResource(request);
    }
    
    connect(reply, &QNetworkReply::finished, this, [reply, nam, enable]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to change Moderator status (enable:" << enable << "):" << reply->errorString() << reply->readAll();
        } else {
            qInfo() << "Moderator status updated successfully (enable:" << enable << ")!";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::banUser(const QString& targetUserId, int duration, const QString& reason) {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot ban/timeout user: Missing Auth Data or Broadcaster ID.";
        return;
    }
    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/moderation/bans?broadcaster_id=%1&moderator_id=%2")
             .arg(m_broadcasterId).arg(m_broadcasterId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject dataObj;
    dataObj["user_id"] = targetUserId;
    if (duration > 0) {
        dataObj["duration"] = duration;
    }
    dataObj["reason"] = reason;
    
    QJsonObject bodyObj;
    bodyObj["data"] = dataObj;
    QJsonDocument doc(bodyObj);
    
    qInfo() << "Banning/Timing-out user:" << targetUserId << "duration:" << duration;
    QNetworkReply* reply = nam->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, [reply, nam]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to ban/timeout user:" << reply->errorString() << reply->readAll();
        } else {
            qInfo() << "User banned/timed-out successfully!";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

void TwitchEventCollectorImpl::unbanUser(const QString& targetUserId) {
    if (m_clientId.isEmpty() || m_accessToken.isEmpty() || m_broadcasterId.isEmpty()) {
        qWarning() << "Cannot unban user: Missing Auth Data or Broadcaster ID.";
        return;
    }
    auto* nam = new QNetworkAccessManager(this);
    QUrl url(QString("https://api.twitch.tv/helix/moderation/bans?broadcaster_id=%1&moderator_id=%2&user_id=%3")
             .arg(m_broadcasterId).arg(m_broadcasterId).arg(targetUserId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    
    qInfo() << "Unbanning user:" << targetUserId;
    QNetworkReply* reply = nam->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [reply, nam]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to unban user:" << reply->errorString() << reply->readAll();
        } else {
            qInfo() << "User unbanned successfully!";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}
