#pragma once
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override;

    // config.json 等からの初期化
    bool loadConfig();
    
    // OAuth 2.0 Implicit Flow を開始する（ブラウザを開く）
    void startOAuthFlow();
    
    QString getClientId() const { return m_clientId; }
    QString getAccessToken() const { return m_accessToken; }

    // 棒読みちゃん設定の Getter/Setter
    QString bouyomiHost() const { return m_bouyomiHost; }
    void setBouyomiHost(const QString& host) { m_bouyomiHost = host; }
    
    int bouyomiPort() const { return m_bouyomiPort; }
    void setBouyomiPort(int port) { m_bouyomiPort = port; }
    
    QString bouyomiExePath() const { return m_bouyomiExePath; }
    void setBouyomiExePath(const QString& path) { m_bouyomiExePath = path; }
    
    int bouyomiVoice() const { return m_bouyomiVoice; }
    void setBouyomiVoice(int voice) { m_bouyomiVoice = voice; }
    
    int bouyomiVolume() const { return m_bouyomiVolume; }
    void setBouyomiVolume(int volume) { m_bouyomiVolume = volume; }
    
    bool getBouyomiAutoStart() const;
    void setBouyomiAutoStart(bool autoStart);
    bool getBouyomiAutoStop() const;
    void setBouyomiAutoStop(bool autoStop);

    // OBS連携設定
    bool getObsFileOutputEnabled() const;
    void setObsFileOutputEnabled(bool enabled);
    bool getObsWebSocketEnabled() const;
    void setObsWebSocketEnabled(bool enabled);

    // 認証情報
    void saveConfig(); // 設定保存用メソッド

signals:
    // 認証処理完了時の通知
    void authCompleted(bool success, const QString& errorMsg);

private slots:
    // ローカルHTTPサーバーの接続・受信ハンドラ
    void onNewConnection();
    void onReadyRead();

private:
    QString m_clientId;
    QString m_redirectUri;
    QString m_accessToken;
    QString m_cipherKey; // TransCipher用の鍵

    // 棒読みちゃん設定
    QString m_bouyomiHost = "127.0.0.1";
    int m_bouyomiPort = 50001;
    QString m_bouyomiExePath = "";
    int m_bouyomiVoice = 0;
    int m_bouyomiVolume = -1;
    bool m_bouyomiAutoStart = false;
    bool m_bouyomiAutoStop = false;

    bool m_obsFileOutputEnabled = false;
    bool m_obsWebSocketEnabled = false;

    QTcpServer* m_httpServer;

    bool loadToken();
    void saveToken(const QString& token);
    void sendHtmlResponse(QTcpSocket* socket, const QString& html);
};
