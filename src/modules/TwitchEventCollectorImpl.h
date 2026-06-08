#pragma once
#include "ITwitchEventCollector.h"
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>

class TwitchEventCollectorImpl : public QObject, public ITwitchEventCollector {
    Q_OBJECT
public:
    explicit TwitchEventCollectorImpl(QObject* eventTarget, QObject* parent = nullptr);
    ~TwitchEventCollectorImpl() override;

    void connectToTwitch() override;
    void disconnectFromTwitch() override;

private slots:
    void onConnected();
    void onTextMessageReceived(const QString& message);
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QWebSocket m_webSocket;
    QObject* m_eventTarget; // イベント送信先（AppController等のハンドラ）
    
    void processNotification(const QJsonObject& json);
};
