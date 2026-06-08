#include "ObsHttpServer.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMimeDatabase>
#include <QCoreApplication>
#include <QDebug>

ObsHttpServer::ObsHttpServer(QObject* parent) 
    : QTcpServer(parent)
    , m_indexFile("overlay.html")
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
            
            // ルートパスへのアクセスは設定されたインデックスファイルにルーティング
            if (path == "/" || path.isEmpty()) {
                path = "/" + m_indexFile;
            }

            // ディレクトリトラバーサル防止の簡易チェック
            if (path.contains("..")) {
                sendNotFound(socket);
                return;
            }

            QString absoluteFilePath = m_documentRoot + path;
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
    response.append("Connection: close\r\n\r\n");
    response.append(content);

    socket->write(response);
    socket->flush();
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
