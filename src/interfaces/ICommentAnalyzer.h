#pragma once
#include <QString>

class ICommentAnalyzer {
public:
    virtual ~ICommentAnalyzer() = default;

    // コメントを解析する
    // userId: 発言者のTwitch ID
    // username: 発言者の名前
    virtual void analyzeComment(const QString& userId, const QString& username, const QString& message) = 0;
    
    // スパム・NGワードを伏せ字 (***) に置き換えた安全な文字列を返す
    virtual QString sanitizeMessage(const QString& message) = 0;
};
