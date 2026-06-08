#pragma once
#include <QString>

class IBouyomiIntegration {
public:
    virtual ~IBouyomiIntegration() = default;
    
    // 初期化 (プロセス起動チェック等)
    virtual void initialize() = 0;
    
    // 棒読みちゃん(TCP)等へのテキスト送信
    virtual void sendText(const QString& text) = 0;
};
