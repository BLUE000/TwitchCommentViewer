#pragma once
#include "../interfaces/IObsIntegration.h"
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>

class ObsWebSocketServer : public QObject, public IObsIntegration {
    Q_OBJECT
public:
    explicit ObsWebSocketServer(quint16 port = 8080, QObject* parent = nullptr);
    ~ObsWebSocketServer() override;

    void sendAction(const QString& actionType, const QVariantMap& payload) override;
    quint16 serverPort() const;

signals:
    void clientConnected();

private slots:
    void onNewConnection();
    void processTextMessage(const QString& message);
    void socketDisconnected();

private:
    QWebSocketServer* m_webSocketServer;
    QList<QWebSocket*> m_clients;
};
