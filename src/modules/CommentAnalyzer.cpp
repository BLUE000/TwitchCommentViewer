#include "CommentAnalyzer.h"
#include <QDebug>

CommentAnalyzer::CommentAnalyzer(QObject* parent) : QObject(parent), m_totalComments(0) {
    initDictionaries();
}

void CommentAnalyzer::setBotUsers(const QStringList& bots) {
    m_botUsers = bots;
}

void CommentAnalyzer::initDictionaries() {
    // 配信・ゲーム実況向けカスタム辞書（静的定義）

    // 1. ポジティブワード
    m_positiveWords = {
        "最高", "神", "草", "たすかる", "助かる", "かわいい", "可愛い", "おもろ", "面白い",
        "ナイス", "GG", "gg", "うおお", "天才", "きちゃ", "てんさい", "うまい", "上手い", "感謝"
    };

    // 2. ネガティブワード
    m_negativeWords = {
        "最悪", "つまらん", "おもんない", "沼", "萎え", "ゴミ", "カス", "下手", "ひどい",
        "終わってる", "草も生えない", "イライラ", "クソ", "くそ", "だる", "だるい"
    };

    // 3. スパム・NGワード（例としてのマイルドなリスト）
    m_spamWords = {
        "死ね", "殺す", "氏ね", "しね", "消えろ", "バカ", "ばか", "アホ", "あほ"
    };

    // 4. トレンド抽出用キーワード（本来は形態素解析で名詞を抜くが、ここではゲーム用語・頻出名詞を事前登録）
    m_trendNouns = {
        "ボス", "ガチャ", "耐久", "クリア", "バグ", "フラグ", "エンディング", "配信", "ゲーム",
        "レベル", "装備", "回復", "主人公", "敵", "罠", "チート", "初心者", "プロ"
    };
}

void CommentAnalyzer::analyzeComment(const QString& userId, const QString& username, const QString& message) {
    if (message.isEmpty()) return;

    // ボットユーザーのチェック（大文字小文字を区別せず比較）
    bool isBot = false;
    for (const QString& bot : m_botUsers) {
        if (bot.trimmed().compare(username, Qt::CaseInsensitive) == 0) {
            isBot = true;
            break;
        }
    }

    if (!isBot) {
        m_totalComments++;
        m_userCommentCount[username]++;
        emit statisticsUpdated(m_totalComments, m_userCommentCount, username, QDateTime::currentDateTime());
    }

    // 1. スパムチェック
    QString spamReason = checkSpam(message);
    if (!spamReason.isEmpty()) {
        emit spamDetected(username, spamReason, message);
        // スパムと判定された場合はトレンドや感情には加味しない
        return;
    }

    // 2. 感情分析スコアの計算 (-1.0 〜 1.0)
    double score = calculateEmotionScore(message);
    if (score != 0.0) {
        emit emotionScored(username, message, score);
    }

    // 3. トレンドワードの抽出と集計
    extractAndCountKeywords(message);
}

QString CommentAnalyzer::checkSpam(const QString& message) {
    // NGワードチェック
    for (const QString& word : m_spamWords) {
        if (message.contains(word, Qt::CaseInsensitive)) {
            return "NGワードが含まれています";
        }
    }
    
    // 文字の連続スパムチェック (例: "ああああああああああああ" などの10文字以上の連続)
    if (message.length() > 20) {
        int maxRepeat = 0;
        int currentRepeat = 1;
        QChar lastChar = message[0];
        
        for (int i = 1; i < message.length(); ++i) {
            if (message[i] == lastChar) {
                currentRepeat++;
                if (currentRepeat > maxRepeat) maxRepeat = currentRepeat;
            } else {
                currentRepeat = 1;
                lastChar = message[i];
            }
        }
        if (maxRepeat >= 10) {
            return "同じ文字の連続が多すぎます";
        }
    }

    return "";
}

QString CommentAnalyzer::sanitizeMessage(const QString& message) {
    QString sanitized = message;
    
    // 1. NGワードの置換
    for (const QString& word : m_spamWords) {
        if (sanitized.contains(word, Qt::CaseInsensitive)) {
            QString replacement;
            replacement.fill('*', word.length());
            sanitized.replace(word, replacement, Qt::CaseInsensitive);
        }
    }
    
    // 2. 文字の連続スパムの省略 (同じ文字が連続する場合は3文字+...にまとめる)
    if (!sanitized.isEmpty()) {
        QString collapsed;
        int currentRepeat = 1;
        QChar lastChar = sanitized[0];
        collapsed.append(lastChar);
        
        for (int i = 1; i < sanitized.length(); ++i) {
            if (sanitized[i] == lastChar) {
                currentRepeat++;
                if (currentRepeat <= 3) {
                    collapsed.append(sanitized[i]);
                } else if (currentRepeat == 4) {
                    collapsed.append("...");
                }
                // 5回目以降の同じ文字はスキップ
            } else {
                currentRepeat = 1;
                lastChar = sanitized[i];
                collapsed.append(lastChar);
            }
        }
        sanitized = collapsed;
    }
    
    return sanitized;
}

double CommentAnalyzer::calculateEmotionScore(const QString& message) {
    int positiveCount = 0;
    int negativeCount = 0;

    for (const QString& word : m_positiveWords) {
        if (message.contains(word, Qt::CaseInsensitive)) {
            positiveCount++;
        }
    }

    for (const QString& word : m_negativeWords) {
        if (message.contains(word, Qt::CaseInsensitive)) {
            negativeCount++;
        }
    }

    // 簡易的なスコア計算
    if (positiveCount == 0 && negativeCount == 0) return 0.0;
    
    // (ポジティブ - ネガティブ) / (ポジティブ + ネガティブ)
    // これにより -1.0(完全にネガティブ) 〜 1.0(完全にポジティブ) に収める
    double total = positiveCount + negativeCount;
    return (positiveCount - negativeCount) / total;
}

void CommentAnalyzer::extractAndCountKeywords(const QString& message) {
    QDateTime now = QDateTime::currentDateTime();

    // 簡易マッチングでトレンド語をカウント
    for (const QString& word : m_trendNouns) {
        if (message.contains(word, Qt::CaseInsensitive)) {
            m_keywordStats[word].count++;
            m_keywordStats[word].lastSeen = now;

            // 一定回数(例: 3回)登場したらトレンドとして通知 (※簡易ロジック)
            if (m_keywordStats[word].count % 3 == 0) {
                emit trendWordDetected(word, m_keywordStats[word].count);
            }
        }
    }

    // 実際にはここで古いトレンド情報をパージするなどの処理が必要
}
