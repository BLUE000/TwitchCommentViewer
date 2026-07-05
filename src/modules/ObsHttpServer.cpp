#include "ObsHttpServer.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMimeDatabase>
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>

ObsHttpServer::ObsHttpServer(QObject* parent) 
    : QTcpServer(parent)
    , m_indexFile("standard/index.html")
{
    m_documentRoot = QCoreApplication::applicationDirPath() + "/assets/overlay";
}

ObsHttpServer::~ObsHttpServer() {
    close();
}

void ObsHttpServer::setDocumentRoot(const QString& path) {
    m_documentRoot = path;
}

void ObsHttpServer::setIndexFile(const QString& filename) {
    m_indexFile = filename;
}

void ObsHttpServer::incomingConnection(qintptr socketDescriptor) {
    QTcpSocket* socket = new QTcpSocket(this);
    if (socket->setSocketDescriptor(socketDescriptor)) {
        connect(socket, &QTcpSocket::readyRead, this, &ObsHttpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    } else {
        delete socket;
    }
}

void ObsHttpServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (socket->canReadLine()) {
        QString requestLine = socket->readLine();
        QStringList tokens = requestLine.split(' ');
        if (tokens.size() >= 2 && tokens[0] == "GET") {
            QString path = tokens[1];

            // 第1の壁: URLパス文字フィルタ (許可文字以外、または連続ドットは即座に拒否)
            QRegularExpression allowedPattern("^[a-zA-Z0-9_\\-\\./]+$");
            if (!allowedPattern.match(path).hasMatch() || path.contains("..")) {
                sendNotFound(socket);
                return;
            }
            
            // ルートパスへのアクセスは設定されたインデックスファイルにルーティング
            if (path == "/" || path.isEmpty()) {
                path = "/" + m_indexFile;
            }

            // 第2の壁: canonicalFilePath検証による物理実絶対パスの範囲チェック
            QString absoluteFilePath = m_documentRoot + path;
            QFileInfo fileInfo(absoluteFilePath);
            if (!fileInfo.exists()) {
                // フォールバック: ルート直下の相対リソースが404の場合、インデックスファイルのサブフォルダ配下を検索する
                if (m_indexFile.contains('/')) {
                    QString subDir = m_indexFile.left(m_indexFile.lastIndexOf('/') + 1); // "physics/" 等
                    QString fallbackPath = m_documentRoot + "/" + subDir + path.mid(1);
                    QFileInfo fallbackInfo(fallbackPath);
                    if (fallbackInfo.exists()) {
                        absoluteFilePath = fallbackPath;
                        fileInfo = fallbackInfo;
                    } else {
                        sendNotFound(socket);
                        return;
                    }
                } else {
                    sendNotFound(socket);
                    return;
                }
            }

            QString canonicalRoot = QDir(m_documentRoot).canonicalPath();
            QString canonicalFile = fileInfo.canonicalFilePath();
            
            // Windows環境では大文字小文字を区別しないファイルシステムに対応するため、大文字小文字無視で比較する
#ifdef Q_OS_WIN
            bool isInside = canonicalFile.startsWith(canonicalRoot, Qt::CaseInsensitive);
#else
            bool isInside = canonicalFile.startsWith(canonicalRoot);
#endif

            if (canonicalFile.isEmpty() || !isInside) {
                sendForbidden(socket);
                return;
            }

            sendResponse(socket, absoluteFilePath);
        }
    }
}

void ObsHttpServer::sendResponse(QTcpSocket* socket, const QString& filePath) {
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        sendNotFound(socket);
        return;
    }

    QByteArray content = file.readAll();
    file.close();

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    QString contentType = mime.name();
    if (filePath.endsWith(".css", Qt::CaseInsensitive)) contentType = "text/css";
    else if (filePath.endsWith(".js", Qt::CaseInsensitive)) contentType = "application/javascript";

    QByteArray response;
    response.append("HTTP/1.1 200 OK\r\n");
    response.append("Content-Type: " + contentType.toUtf8() + "\r\n");
    response.append("Content-Length: " + QByteArray::number(content.size()) + "\r\n");
    // 第3の壁: CSP（Content-Security-Policy）ヘッダーの強制適用
    response.append("Content-Security-Policy: default-src 'self' http://localhost:* ws://localhost:* http://127.0.0.1:* ws://127.0.0.1:* ws: wss: https://fonts.googleapis.com https://fonts.gstatic.com; img-src 'self' data: https://static-cdn.jtvnw.net; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com;\r\n");
    response.append("Connection: close\r\n\r\n");
    response.append(content);

    socket->write(response);
    socket->flush();
    
    // 送信バッファのデータが完全に送信されるまで待機（LAN遅延対策）
    if (socket->bytesToWrite() > 0) {
        socket->waitForBytesWritten(3000); // 最大3秒待機
    }
    socket->disconnectFromHost();
}

void ObsHttpServer::sendNotFound(QTcpSocket* socket) {
    QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: close\r\n\r\n"
                          "404 Not Found";
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void ObsHttpServer::sendForbidden(QTcpSocket* socket) {
    QByteArray response = "HTTP/1.1 403 Forbidden\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: close\r\n\r\n"
                          "403 Forbidden";
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}
