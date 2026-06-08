#pragma once
#include <QString>

class ITwitchEventCollector {
public:
    virtual ~ITwitchEventCollector() = default;
    
    // Twitch EventSub (WebSocket) への接続開始
    virtual void connectToTwitch() = 0;
    
    // 接続の終了
    virtual void disconnectFromTwitch() = 0;
};
