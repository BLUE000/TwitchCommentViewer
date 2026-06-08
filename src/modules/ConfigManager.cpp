#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include <QCoreApplication>
#include <cipher_engine.h>

ConfigManager::ConfigManager(QObject* parent) 
    : QObject(parent), m_httpServer(new QTcpServer(this)) 
{
    // アプリケーション起動ごとのセキュアな固定鍵 (実運用ではOSのKeychain等から取得するのが理想)
    // v2: スコープ変更に伴い、旧バージョンのトークンを無効化して再認証を強制する
    m_cipherKey = "twitch_oauth_secure_key_v2";

    connect(m_httpServer, &QTcpServer::newConnection, this, &ConfigManager::onNewConnection);
}

ConfigManager::~ConfigManager() {
    if (m_httpServer->isListening()) {
        m_httpServer->close();
    }
}

bool ConfigManager::loadConfig() {
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json";
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config.json at" << configPath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;

    QJsonObject json = doc.object();
    
    // 既存のキー名との互換性
    if (json.contains("twitch_client_id")) m_clientId = json["twitch_client_id"].toString();
    else if (json.contains("clientId")) m_clientId = json["clientId"].toString();
    
    if (json.contains("twitch_redirect_uri")) m_redirectUri = json["twitch_redirect_uri"].toString();
    else if (json.contains("redirectUri")) m_redirectUri = json["redirectUri"].toString();
    
    // 棒読みちゃん設定の読み込み
    QJsonObject bouyomiObj = json["bouyomi"].toObject();
    if (bouyomiObj.contains("bouyomiHost")) m_bouyomiHost = bouyomiObj["bouyomiHost"].toString();
    if (bouyomiObj.contains("bouyomiPort")) m_bouyomiPort = bouyomiObj["bouyomiPort"].toInt();
    if (bouyomiObj.contains("bouyomiExePath")) m_bouyomiExePath = bouyomiObj["bouyomiExePath"].toString();
    if (bouyomiObj.contains("bouyomiVoice")) m_bouyomiVoice = bouyomiObj["bouyomiVoice"].toInt();
    if (bouyomiObj.contains("bouyomiVolume")) m_bouyomiVolume = bouyomiObj["bouyomiVolume"].toInt();
    if (bouyomiObj.contains("bouyomiAutoStart")) m_bouyomiAutoStart = bouyomiObj["bouyomiAutoStart"].toBool();
    if (bouyomiObj.contains("auto_stop")) {
        m_bouyomiAutoStop = bouyomiObj["auto_stop"].toBool();
    }
    
    QJsonObject obsObj = json["obs"].toObject();
    if (obsObj.contains("file_output")) {
        m_obsFileOutputEnabled = obsObj["file_output"].toBool();
    }
    if (obsObj.contains("websocket")) {
        m_obsWebSocketEnabled = obsObj["websocket"].toBool();
    }

    // 認証情報の復号化と読み込み
    loadToken();

    return !m_clientId.isEmpty() && !m_redirectUri.isEmpty();
}

void ConfigManager::saveConfig() {
    QString configPath = QCoreApplication::applicationDirPath() + "/config.json";
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config.json for writing.";
        return;
    }

    QJsonObject rootObj;
    rootObj["twitch_client_id"] = m_clientId;
    rootObj["twitch_redirect_uri"] = m_redirectUri;
    
    QJsonObject bouyomiObj;
    bouyomiObj["bouyomiHost"] = m_bouyomiHost;
    bouyomiObj["bouyomiPort"] = m_bouyomiPort;
    bouyomiObj["bouyomiExePath"] = m_bouyomiExePath;
    bouyomiObj["bouyomiVoice"] = m_bouyomiVoice;
    bouyomiObj["bouyomiVolume"] = m_bouyomiVolume;
    bouyomiObj["auto_start"] = m_bouyomiAutoStart;
    bouyomiObj["auto_stop"] = m_bouyomiAutoStop;
    rootObj["bouyomi"] = bouyomiObj;

    QJsonObject obsObj;
    obsObj["file_output"] = m_obsFileOutputEnabled;
    obsObj["websocket"] = m_obsWebSocketEnabled;
    rootObj["obs"] = obsObj;

    // トークン情報の暗号化と保存
    QJsonDocument doc(rootObj);
    file.write(doc.toJson());
}

void ConfigManager::startOAuthFlow() {
    // 既にトークンがあればスキップ
    if (!m_accessToken.isEmpty()) {
        qInfo() << "Access token already loaded. Skipping OAuth flow.";
        emit authCompleted(true, "");
        return;
    }

    // ローカルサーバーを起動 (例: http://localhost:30080/)
    QUrl redirectUrl(m_redirectUri);
    int port = redirectUrl.port(30080);
    
    if (!m_httpServer->isListening()) {
        if (!m_httpServer->listen(QHostAddress::LocalHost, port)) {
            emit authCompleted(false, "Failed to start local HTTP server on port " + QString::number(port));
            return;
        }
    }

    // ブラウザを開いてTwitchの認証画面へ誘導 (Implicit Grant Flow)
    // ※ EventSub のチャット取得には user:read:chat スコープが必須 (空白はURLエンコードが必要)
    QString authUrl = QString("https://id.twitch.tv/oauth2/authorize"
                              "?response_type=token"
                              "&client_id=%1"
                              "&redirect_uri=%2"
                              "&scope=user:read:chat%20user:write:chat")
                          .arg(m_clientId)
                          .arg(m_redirectUri);
                          
    QDesktopServices::openUrl(QUrl(authUrl));
    qInfo() << "Opened browser for OAuth authorization. Waiting for redirect...";
}

void ConfigManager::onNewConnection() {
    QTcpSocket* socket = m_httpServer->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &ConfigManager::onReadyRead);
}

void ConfigManager::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray request = socket->readAll();
    QString requestStr(request);

    // GETリクエストの1行目を解析 (例: "GET / HTTP/1.1" または "GET /token?access_token=... HTTP/1.1")
    QStringList lines = requestStr.split("\r\n");
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }

    QString firstLine = lines.first();
    QStringList parts = firstLine.split(" ");
    if (parts.size() < 2) {
        socket->disconnectFromHost();
        return;
    }

    QString path = parts[1]; // Request URI

    if (path == "/") {
        // デスクトップアプリのImplicit Flowの罠：アクセストークンは # (フラグメント) に入るため、サーバーに送信されない。
        // 回避策として、フロントエンドのJSでフラグメントを読み取り、再度サーバーへ投げるHTMLを返す。
        QString html = 
            "<html><body>"
            "<h2>Authentication in progress...</h2>"
            "<script>"
            "if (window.location.hash) {"
            "    window.location.href = '/token?' + window.location.hash.substring(1);"
            "} else {"
            "    document.body.innerHTML = '<h2>Error: No token received.</h2>';"
            "}"
            "</script>"
            "</body></html>";
        sendHtmlResponse(socket, html);
    } 
    else if (path.startsWith("/token?")) {
        // JSによってリダイレクトされてきたトークンを受け取る
        QUrl url("http://localhost" + path);
        QUrlQuery query(url);
        
        m_accessToken = query.queryItemValue("access_token");
        
        if (!m_accessToken.isEmpty()) {
            saveToken(m_accessToken);
            sendHtmlResponse(socket, "<html><body><h2>Authentication Successful!</h2><p>You can close this window and return to the application.</p><script>window.close();</script></body></html>");
            emit authCompleted(true, "");
            
            // サーバー停止
            m_httpServer->close();
        } else {
            sendHtmlResponse(socket, "<html><body><h2>Authentication Failed.</h2></body></html>");
            emit authCompleted(false, "No access_token found in redirect.");
        }
    } else {
        // 404
        socket->write("HTTP/1.1 404 Not Found\r\n\r\n");
        socket->disconnectFromHost();
    }
}

void ConfigManager::sendHtmlResponse(QTcpSocket* socket, const QString& html) {
    QString response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Connection: close\r\n\r\n" + html;
    
    socket->write(response.toUtf8());
    socket->flush();
    socket->waitForBytesWritten(3000);
    socket->disconnectFromHost();
}

void ConfigManager::saveToken(const QString& token) {
    // トークンを平文で保存するのは危険なので TransCipher で暗号化する
    CipherResult res = CipherEngine::encrypt(token.toUtf8(), m_cipherKey);
    if (res.isSuccess()) {
        QString tokenPath = QCoreApplication::applicationDirPath() + "/tokens.enc";
        QFile file(tokenPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(res.data());
        }
    }
}

bool ConfigManager::getBouyomiAutoStart() const {
    return m_bouyomiAutoStart;
}

void ConfigManager::setBouyomiAutoStart(bool autoStart) {
    m_bouyomiAutoStart = autoStart;
}

bool ConfigManager::getBouyomiAutoStop() const {
    return m_bouyomiAutoStop;
}

void ConfigManager::setBouyomiAutoStop(bool autoStop) {
    m_bouyomiAutoStop = autoStop;
}

bool ConfigManager::getObsFileOutputEnabled() const {
    return m_obsFileOutputEnabled;
}

void ConfigManager::setObsFileOutputEnabled(bool enabled) {
    m_obsFileOutputEnabled = enabled;
}

bool ConfigManager::getObsWebSocketEnabled() const {
    return m_obsWebSocketEnabled;
}

void ConfigManager::setObsWebSocketEnabled(bool enabled) {
    m_obsWebSocketEnabled = enabled;
}

bool ConfigManager::loadToken() {
    QString tokenPath = QCoreApplication::applicationDirPath() + "/tokens.enc";
    QFile file(tokenPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray encData = file.readAll();
    CipherResult res = CipherEngine::decrypt(encData, m_cipherKey);
    if (res.isSuccess()) {
        m_accessToken = QString::fromUtf8(res.data());
        return true;
    }
    return false;
}
