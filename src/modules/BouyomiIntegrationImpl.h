#pragma once
#include "../interfaces/ITtsIntegration.h"
#include <QObject>
#include <QTcpSocket>
#include <QProcess>

class ConfigManager;

class BouyomiIntegrationImpl : public QObject, public ITtsIntegration {
    Q_OBJECT
public:
    explicit BouyomiIntegrationImpl(ConfigManager* config, QObject* parent = nullptr);
    ~BouyomiIntegrationImpl() override;

    void initialize() override;
    void sendText(const QString& text) override;

    // 起動・終了管理
    void checkAndStartProcess();
    void stopProcess();

private:
    ConfigManager* m_config;
    QProcess* m_process;
};
