#pragma once

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>

class ObsHttpServer : public QTcpServer {
    Q_OBJECT

public:
    explicit ObsHttpServer(QObject* parent = nullptr);
    ~ObsHttpServer() override;

    void setDocumentRoot(const QString& path);
    void setIndexFile(const QString& filename);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onReadyRead();

private:
    void sendResponse(QTcpSocket* socket, const QString& filePath);
    void sendNotFound(QTcpSocket* socket);

    QString m_documentRoot;
    QString m_indexFile;
};
