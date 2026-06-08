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
}

void ObsWebSocketServer::processTextMessage(const QString& message) {
    // クライアントからのメッセージは基本受け取らないが、将来的な双方向通信用にプレースホルダ
    qDebug() << "Message received from OBS:" << message;
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
