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

    QTcpServer* m_httpServer;

    bool loadToken();
    void saveToken(const QString& token);
    void sendHtmlResponse(QTcpSocket* socket, const QString& html);
};
