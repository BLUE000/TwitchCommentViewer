#pragma once
#include <QString>
#include <QVariantMap>

class IObsIntegration {
public:
    virtual ~IObsIntegration() = default;
    
    // OBS WebSocket 経由でのアクション送信
    virtual void sendAction(const QString& actionType, const QVariantMap& payload) = 0;
};
