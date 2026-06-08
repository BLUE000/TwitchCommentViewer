#pragma once
#include "../interfaces/ICommentAnalyzer.h"
#include <QObject>
#include <QStringList>
#include <QMap>
#include <QDateTime>

class CommentAnalyzer : public QObject, public ICommentAnalyzer {
    Q_OBJECT
public:
    explicit CommentAnalyzer(QObject* parent = nullptr);
    ~CommentAnalyzer() override = default;

    void analyzeComment(const QString& userId, const QString& username, const QString& message) override;

signals:
    // 解析結果の通知シグナル
    void spamDetected(const QString& username, const QString& reason, const QString& message);
    void trendWordDetected(const QString& word, int count);
    void emotionScored(const QString& username, const QString& message, double score);

private:
    void initDictionaries();
    double calculateEmotionScore(const QString& message);
    QString checkSpam(const QString& message);
    void extractAndCountKeywords(const QString& message);

    // 辞書
    QStringList m_positiveWords;
    QStringList m_negativeWords;
    QStringList m_spamWords;
    QStringList m_trendNouns; // トレンド抽出対象となりうる名詞やゲーム用語

    // 状態管理
    struct KeywordCount {
        int count;
        QDateTime lastSeen;
    };
    QMap<QString, KeywordCount> m_keywordStats; // 単語の出現回数（直近のトレンド計算用）
    
    // ユーザ統計 (簡易メモリキャッシュ)
    QMap<QString, int> m_userCommentCount;
};
