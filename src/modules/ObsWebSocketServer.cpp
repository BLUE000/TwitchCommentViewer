#include "ObsWebSocketServer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ObsWebSocketServer::ObsWebSocketServer(quint16 port, QObject* parent) 
    : QObject(parent), m_webSocketServer(new QWebSocketServer(QStringLiteral("Obs Integration Server"), QWebSocketServer::NonSecureMode, this))
{
    if (m_webSocketServer->listen(QHostAddress::Any, port)) {
        qInfo() << "OBS WebSocket Server listening on port" << port;
        connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &ObsWebSocketServer::onNewConnection);
    } else {
        qWarning() << "OBS WebSocket Server failed to start on port" << port;
    }
}

ObsWebSocketServer::~ObsWebSocketServer() {
    m_webSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void ObsWebSocketServer::onNewConnection() {
    QWebSocket* pSocket = m_webSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &ObsWebSocketServer::processTextMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &ObsWebSocketServer::socketDisconnected);
    
    m_clients << pSocket;
    qInfo() << "OBS Browser Source connected.";
    emit clientConnected();
}

void ObsWebSocketServer::processTextMessage(const QString& message) {
    QWebSocket* pSender = qobject_cast<QWebSocket*>(sender());
    if (!pSender) return;

    // JSONをパースしてアクション名を確認
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        QString action = obj["action"].toString();
        
        if (action == "drag_start" || action == "drag_move" || action == "drag_end" || action == "settings_changed" || action == "double_click") {
            for (QWebSocket* pClient : std::as_const(m_clients)) {
                if (pClient != pSender) {
                    pClient->sendTextMessage(message);
                }
            }
        }
    }
}

void ObsWebSocketServer::socketDisconnected() {
    QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
        qInfo() << "OBS Browser Source disconnected.";
    }
}

void ObsWebSocketServer::sendAction(const QString& actionType, const QVariantMap& payload) {
    // ペイロードをJSONにして全クライアントへ配信
    QJsonObject jsonObj = QJsonObject::fromVariantMap(payload);
    jsonObj["action"] = actionType;
    
    QJsonDocument doc(jsonObj);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);

    for (QWebSocket* pClient : std::as_const(m_clients)) {
        pClient->sendTextMessage(jsonStr);
    }
}

quint16 ObsWebSocketServer::serverPort() const {
    return m_webSocketServer ? m_webSocketServer->serverPort() : 0;
}
