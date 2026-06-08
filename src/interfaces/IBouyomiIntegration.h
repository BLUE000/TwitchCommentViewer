#pragma once
#include <QString>

class IBouyomiIntegration {
public:
    virtual ~IBouyomiIntegration() = default;
    
    // 棒読みちゃん(TCP)等へのテキスト送信
    virtual void sendText(const QString& text) = 0;
};
