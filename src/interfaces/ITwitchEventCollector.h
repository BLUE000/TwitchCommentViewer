#pragma once
#include <QString>

class ITwitchEventCollector {
public:
    virtual ~ITwitchEventCollector() = default;
    
    // Twitch EventSub (WebSocket) への接続開始
    virtual void connectToTwitch() = 0;
    
    // 接続の終了
    virtual void disconnectFromTwitch() = 0;

    // 視聴者リストの非同期取得を要求する
    virtual void requestChatters() = 0;

    // 新規追加: モデレーション関連API
    virtual void sendShoutout(const QString& toBroadcasterId) = 0;
    virtual void setVipStatus(const QString& targetUserId, bool enable) = 0;
    virtual void setModeratorStatus(const QString& targetUserId, bool enable) = 0;
    virtual void banUser(const QString& targetUserId, int duration, const QString& reason = "") = 0;
    virtual void unbanUser(const QString& targetUserId) = 0;
};
