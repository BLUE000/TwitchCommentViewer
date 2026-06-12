#include "VoiceVoxIntegrationImpl.h"
#include "ConfigManager.h"
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

VoiceVoxIntegrationImpl::VoiceVoxIntegrationImpl(ConfigManager* config, QObject* parent)
    : QObject(parent), m_config(config), m_networkManager(new QNetworkAccessManager(this)), m_process(new QProcess(this))
{
}

VoiceVoxIntegrationImpl::~VoiceVoxIntegrationImpl() {
    stopProcess();
}

void VoiceVoxIntegrationImpl::initialize() {
    checkAndStartProcess();
}

void VoiceVoxIntegrationImpl::sendText(const QString& text) {
    if (text.isEmpty()) return;

    QString host = m_config ? m_config->voicevoxHost() : "127.0.0.1";
    int port = m_config ? m_config->voicevoxPort() : 50021;
    int speaker = m_config ? m_config->voicevoxSpeaker() : 3;

    QUrl url(QString("http://%1:%2/audio_query").arg(host).arg(port));
    QUrlQuery query;
    query.addQueryItem("text", text);
    query.addQueryItem("speaker", QString::number(speaker));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply* reply = m_networkManager->post(request, QByteArray());
    
    connect(reply, &QNetworkReply::finished, this, [this, host, port, speaker]() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "VOICEVOX audio_query error:" << reply->errorString();
            return;
        }

        QByteArray queryData = reply->readAll();
        QByteArray modifiedData = modifyQueryJson(queryData);

        QUrl synthesisUrl(QString("http://%1:%2/synthesis").arg(host).arg(port));
        QUrlQuery synthQuery;
        synthQuery.addQueryItem("speaker", QString::number(speaker));
        synthesisUrl.setQuery(synthQuery);

        QNetworkRequest synthRequest(synthesisUrl);
        synthRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* synthReply = m_networkManager->post(synthRequest, modifiedData);
        connect(synthReply, &QNetworkReply::finished, this, [this]() {
            QNetworkReply* synthReply = qobject_cast<QNetworkReply*>(sender());
            if (!synthReply) return;
            synthReply->deleteLater();
            if (synthReply->error() != QNetworkReply::NoError) {
                qWarning() << "VOICEVOX synthesis error:" << synthReply->errorString();
                return;
            }

            QByteArray wavData = synthReply->readAll();
            if (wavData.isEmpty()) {
                qWarning() << "VOICEVOX synthesis returned empty audio data";
                return;
            }

#ifdef Q_OS_WIN
            // 現在再生中のサウンドを停止
            PlaySoundA(nullptr, nullptr, 0);

            // メモリバッファをメンバ変数にコピーして再生中の生存を確保
            m_currentSoundData = wavData;

            // 非同期でメモリ再生
            PlaySoundA(reinterpret_cast<LPCSTR>(m_currentSoundData.constData()), nullptr, SND_MEMORY | SND_ASYNC);
#else
            qDebug() << "WAV playback from memory is only supported on Windows in this implementation. Received bytes:" << wavData.size();
#endif
        });
    });
}

QByteArray VoiceVoxIntegrationImpl::modifyQueryJson(const QByteArray& queryJson) {
    QJsonDocument doc = QJsonDocument::fromJson(queryJson);
    if (!doc.isObject()) {
        qWarning() << "VOICEVOX audio_query response is not a valid JSON object";
        return queryJson;
    }

    QJsonObject queryObj = doc.object();
    if (m_config) {
        queryObj["speedScale"] = m_config->voicevoxSpeed();
        queryObj["pitchScale"] = m_config->voicevoxPitch();
        queryObj["volumeScale"] = static_cast<double>(m_config->voicevoxVolume()) / 100.0;
    }

    QJsonDocument modifiedDoc(queryObj);
    return modifiedDoc.toJson(QJsonDocument::Compact);
}

void VoiceVoxIntegrationImpl::checkAndStartProcess() {
    if (!m_config) return;
    if (!m_config->getTtsAutoStart()) return;

    QString exePath = m_config->voicevoxExePath();
    if (exePath.isEmpty()) return;

    if (m_process->state() == QProcess::NotRunning) {
        qInfo() << "Starting VOICEVOX:" << exePath;
        m_process->start(exePath, QStringList());
    }
}

void VoiceVoxIntegrationImpl::stopProcess() {
#ifdef Q_OS_WIN
    // サウンド再生停止
    PlaySoundA(nullptr, nullptr, 0);
#endif

    if (!m_config) return;
    if (!m_config->getTtsAutoStop()) return;

    if (m_process->state() != QProcess::NotRunning) {
        qInfo() << "Stopping VOICEVOX...";
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}
