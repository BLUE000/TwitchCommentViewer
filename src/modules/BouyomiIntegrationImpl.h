#pragma once
#include "../interfaces/IBouyomiIntegration.h"
#include <QObject>
#include <QTcpSocket>

class BouyomiIntegrationImpl : public QObject, public IBouyomiIntegration {
    Q_OBJECT
public:
    explicit BouyomiIntegrationImpl(QObject* parent = nullptr);
    ~BouyomiIntegrationImpl() override;

    void sendText(const QString& text) override;

private:
    QString m_host;
    quint16 m_port;
};
