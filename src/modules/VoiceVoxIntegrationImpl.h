#pragma once
#include "../interfaces/ITtsIntegration.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QByteArray>

class ConfigManager;

class VoiceVoxIntegrationImpl : public QObject, public ITtsIntegration {
    Q_OBJECT
public:
    explicit VoiceVoxIntegrationImpl(ConfigManager* config, QObject* parent = nullptr);
    ~VoiceVoxIntegrationImpl() override;

    void initialize() override;
    void sendText(const QString& text) override;

    // クエリJSON書換え処理 (テスト検証用公開メソッド)
    QByteArray modifyQueryJson(const QByteArray& queryJson);

    // 起動・終了管理
    void checkAndStartProcess();
    void stopProcess();

private:
    ConfigManager* m_config;
    QNetworkAccessManager* m_networkManager;
    QProcess* m_process;
    QByteArray m_currentSoundData; // 再生中のWAVデータを保持
};
