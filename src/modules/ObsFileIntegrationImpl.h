#pragma once
#include "../interfaces/IObsIntegration.h"
#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>

class ObsFileIntegrationImpl : public QObject, public IObsIntegration {
    Q_OBJECT
public:
    explicit ObsFileIntegrationImpl(QObject* parent = nullptr);
    ~ObsFileIntegrationImpl() override = default;

    void sendAction(const QString& actionType, const QVariantMap& payload) override;

private:
    QString m_filePath;
    QMutex m_mutex;
};
