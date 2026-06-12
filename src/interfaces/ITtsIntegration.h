#pragma once
#include <QString>

class ITtsIntegration {
public:
    virtual ~ITtsIntegration() = default;
    
    // 初期化 (プロセス起動チェック等)
    virtual void initialize() = 0;
    
    // 音声読み上げエンジンへのテキスト送信
    virtual void sendText(const QString& text) = 0;
};
