#include "BouyomiIntegrationImpl.h"
#include "ConfigManager.h"
#include <QDataStream>
#include <QDebug>

BouyomiIntegrationImpl::BouyomiIntegrationImpl(ConfigManager* config, QObject* parent)
    : QObject(parent), m_config(config), m_process(new QProcess(this))
{
}

BouyomiIntegrationImpl::~BouyomiIntegrationImpl() {
    stopProcess();
}

void BouyomiIntegrationImpl::initialize() {
    checkAndStartProcess();
}

void BouyomiIntegrationImpl::sendText(const QString& text) {
    if (text.isEmpty()) return;

    QByteArray textData = text.toUtf8();
    
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian); // 棒読みちゃんはリトルエンディアン

    stream << static_cast<qint16>(0x0001);  // Command: 1 (読み上げ)
    stream << static_cast<qint16>(-1);      // Speed: -1 (デフォルト)
    stream << static_cast<qint16>(-1);      // Tone: -1 (デフォルト)
    
    int volume = m_config ? m_config->bouyomiVolume() : -1;
    stream << static_cast<qint16>(volume);  // Volume
    
    int voice = m_config ? m_config->bouyomiVoice() : 0;
    stream << static_cast<qint16>(voice);   // Voice
    
    stream << static_cast<qint8>(0);        // CharCode: 0 (UTF-8)
    stream << static_cast<qint32>(textData.size()); // Length

    payload.append(textData);

    // 毎回新しくソケットを作って送信し、閉じたら破棄する（短寿命接続）
    QTcpSocket* socket = new QTcpSocket(this);
    
    connect(socket, &QTcpSocket::connected, this, [socket, payload]() {
        socket->write(payload);
        socket->flush();
        socket->disconnectFromHost();
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, [socket](QAbstractSocket::SocketError error) {
        qWarning() << "Bouyomi-chan connection error:" << error << socket->errorString();
        socket->deleteLater();
    });

    QString host = m_config ? m_config->bouyomiHost() : "127.0.0.1";
    int port = m_config ? m_config->bouyomiPort() : 50001;
    socket->connectToHost(host, port);
}

void BouyomiIntegrationImpl::checkAndStartProcess() {
    if (!m_config) return;
    if (!m_config->getBouyomiAutoStart()) return;
    
    QString exePath = m_config->bouyomiExePath();
    if (exePath.isEmpty()) return;
    
    if (m_process->state() == QProcess::NotRunning) {
        qInfo() << "Starting Bouyomi-chan:" << exePath;
        m_process->start(exePath, QStringList());
    }
}

void BouyomiIntegrationImpl::stopProcess() {
    if (!m_config) return;
    if (!m_config->getBouyomiAutoStop()) return;
    
    if (m_process->state() != QProcess::NotRunning) {
        qInfo() << "Stopping Bouyomi-chan...";
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}
