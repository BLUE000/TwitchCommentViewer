#pragma once
#include <QString>

class ICommentAnalyzer {
public:
    virtual ~ICommentAnalyzer() = default;

    // コメントを解析する
    // userId: 発言者のTwitch ID
    // username: 発言者の名前
    // message: 発言内容
    virtual void analyzeComment(const QString& userId, const QString& username, const QString& message) = 0;
};
