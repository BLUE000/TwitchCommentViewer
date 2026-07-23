#pragma once
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSet>

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
    void setClientId(const QString& id) { m_clientId = id; }
    QString getRedirectUri() const { return m_redirectUri; }
    void setRedirectUri(const QString& uri) { m_redirectUri = uri; }
    QString getAccessToken() const { return m_accessToken; }

    // TTS共通設定の Getter/Setter
    int activeTtsEngine() const { return m_activeTtsEngine; }
    void setActiveTtsEngine(int engine) { m_activeTtsEngine = engine; }

    bool getTtsAutoStart() const { return m_ttsAutoStart; }
    void setTtsAutoStart(bool autoStart) { m_ttsAutoStart = autoStart; }
    bool getTtsAutoStop() const { return m_ttsAutoStop; }
    void setTtsAutoStop(bool autoStop) { m_ttsAutoStop = autoStop; }

    // 互換性維持のためのGetter/Setter
    bool getBouyomiAutoStart() const { return getTtsAutoStart(); }
    void setBouyomiAutoStart(bool autoStart) { setTtsAutoStart(autoStart); }
    bool getBouyomiAutoStop() const { return getTtsAutoStop(); }
    void setBouyomiAutoStop(bool autoStop) { setTtsAutoStop(autoStop); }

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

    int bouyomiSpeed() const { return m_bouyomiSpeed; }
    void setBouyomiSpeed(int speed) { m_bouyomiSpeed = speed; }

    int bouyomiPitch() const { return m_bouyomiPitch; }
    void setBouyomiPitch(int pitch) { m_bouyomiPitch = pitch; }

    // VOICEVOX設定の Getter/Setter
    QString voicevoxHost() const { return m_voicevoxHost; }
    void setVoicevoxHost(const QString& host) { m_voicevoxHost = host; }

    int voicevoxPort() const { return m_voicevoxPort; }
    void setVoicevoxPort(int port) { m_voicevoxPort = port; }

    QString voicevoxExePath() const { return m_voicevoxExePath; }
    void setVoicevoxExePath(const QString& path) { m_voicevoxExePath = path; }

    int voicevoxSpeaker() const { return m_voicevoxSpeaker; }
    void setVoicevoxSpeaker(int speaker) { m_voicevoxSpeaker = speaker; }

    int voicevoxVolume() const { return m_voicevoxVolume; }
    void setVoicevoxVolume(int volume) { m_voicevoxVolume = volume; }

    double voicevoxSpeed() const { return m_voicevoxSpeed; }
    void setVoicevoxSpeed(double speed) { m_voicevoxSpeed = speed; }

    double voicevoxPitch() const { return m_voicevoxPitch; }
    void setVoicevoxPitch(double pitch) { m_voicevoxPitch = pitch; }

    // OBS連携設定
    bool getObsFileOutputEnabled() const;
    void setObsFileOutputEnabled(bool enabled);
    bool getObsWebSocketEnabled() const;
    void setObsWebSocketEnabled(bool enabled);

    int getObsServerPort() const;
    void setObsServerPort(int port);

    QString getObsOverlayFile() const;
    void setObsOverlayFile(const QString& filename);

    int getObsAvatarMinSize() const;
    void setObsAvatarMinSize(int size);
    int getObsAvatarMaxSize() const;
    void setObsAvatarMaxSize(int size);
    int getObsBounceFactor() const;
    void setObsBounceFactor(int factor);

    int getObsBrowserWidth() const;
    void setObsBrowserWidth(int width);
    int getObsBrowserHeight() const;
    void setObsBrowserHeight(int height);

    QString getObsEffectSymbols() const;
    void setObsEffectSymbols(const QString& symbols);
    int getObsEffectSize() const;
    void setObsEffectSize(int size);
    int getObsEffectCount() const;
    void setObsEffectCount(int count);
    int getObsEffectOpacity() const;
    void setObsEffectOpacity(int opacity);

    // ボットユーザー設定
    QStringList getBotUsers() const;
    void setBotUsers(const QStringList& bots);

    // 読み上げ除外ユーザー設定
    QStringList getTtsIgnoreUsers() const;
    void setTtsIgnoreUsers(const QStringList& users);

    // アーティストユーザー設定
    bool isArtist(const QString& userId) const;
    void addArtist(const QString& userId);
    void removeArtist(const QString& userId);

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

    // TTS共通設定
    int m_activeTtsEngine = 1; // 0: None, 1: Bouyomi, 2: VOICEVOX
    bool m_ttsAutoStart = false;
    bool m_ttsAutoStop = false;

    // 棒読みちゃん設定
    QString m_bouyomiHost = "127.0.0.1";
    int m_bouyomiPort = 50001;
    QString m_bouyomiExePath = "";
    int m_bouyomiVoice = 0;
    int m_bouyomiVolume = -1;
    int m_bouyomiSpeed = 100;
    int m_bouyomiPitch = 100;

    // VOICEVOX設定
    QString m_voicevoxHost = "127.0.0.1";
    int m_voicevoxPort = 50021;
    QString m_voicevoxExePath = "";
    int m_voicevoxSpeaker = 3;
    int m_voicevoxVolume = 100;
    double m_voicevoxSpeed = 1.0;
    double m_voicevoxPitch = 0.0;

    bool m_obsFileOutputEnabled = false;
    bool m_obsWebSocketEnabled = false;
    int m_obsServerPort = 8081;
    QString m_obsOverlayFile = "standard/index.html";
    int m_obsAvatarMinSize = 50;
    int m_obsAvatarMaxSize = 150;
    int m_obsBounceFactor = 30;
    int m_obsBrowserWidth = 800;
    int m_obsBrowserHeight = 600;
    QString m_obsEffectSymbols = "♥,♦,♣,♠";
    int m_obsEffectSize = 20;
    int m_obsEffectCount = 5;
    int m_obsEffectOpacity = 100;

    QStringList m_botUsers;
    QStringList m_ttsIgnoreUsers;
    QSet<QString> m_artistUserIds;

    QTcpServer* m_httpServer;

    bool loadToken();
    void saveToken(const QString& token);
    void sendHtmlResponse(QTcpSocket* socket, const QString& html);
};
