#pragma once
#include "../interfaces/IBouyomiIntegration.h"
#include <QObject>
#include <QTcpSocket>

class ConfigManager;

class BouyomiIntegrationImpl : public QObject, public IBouyomiIntegration {
    Q_OBJECT
public:
    explicit BouyomiIntegrationImpl(ConfigManager* config, QObject* parent = nullptr);
    ~BouyomiIntegrationImpl() override;

    void sendText(const QString& text) override;

private:
    ConfigManager* m_config;
};
