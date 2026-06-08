#pragma once
#include <QString>

class ITwitchActionSender {
public:
    virtual ~ITwitchActionSender() = default;
    
    // コメント送信
    virtual void sendChatMessage(const QString& message) = 0;
    
    // チャンネルポイントのステータス更新 (fulfill or reject)
    virtual void updateChannelPointStatus(const QString& redemptionId, bool fulfill) = 0;
};
