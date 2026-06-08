#include "ObsFileIntegrationImpl.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

ObsFileIntegrationImpl::ObsFileIntegrationImpl(QObject* parent) : QObject(parent) {
    m_filePath = QCoreApplication::applicationDirPath() + "/obs_latest_comment.txt";
}

void ObsFileIntegrationImpl::sendAction(const QString& actionType, const QVariantMap& payload) {
    if (actionType != "comment") {
        return; // 今のところコメント情報のみを対象とする
    }

    QString username = payload.value("username").toString();
    QString message = payload.value("message").toString();

    // ファイル書き込み (排他制御)
    QMutexLocker locker(&m_mutex);
    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // 例: "ユーザー名: メッセージ内容" の形式で保存
        QString output = QString("%1: %2\n").arg(username, message);
        file.write(output.toUtf8());
        file.close();
    } else {
        qWarning() << "Failed to write to OBS text file:" << m_filePath;
    }
}
