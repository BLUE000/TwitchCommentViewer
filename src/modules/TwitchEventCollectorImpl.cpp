#include "TwitchEventCollectorImpl.h"
#include "../events/TwitchEvents.h"
#include <QCoreApplication>
#include <QDebug>

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
        qInfo() << "Session Welcome received from Twitch.";
        // QJsonObject payload = json["payload"].toObject();
        // QString sessionId = payload["session"].toObject()["id"].toString();
        // TODO: Subscription登録API呼び出し (TwitchActionSender経由等)
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
