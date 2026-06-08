#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QDateTime>
#include <QList>

struct CommentLog {
    qint64 id;
    QDateTime timestamp;
    QString userId;
    QString userName; // 復号済みの名前
};

// UT-DB-01 および要件定義に基づくSQLiteとTransCipher連携DB管理
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // データベース初期化 (DBファイルパスとTransCipher用暗号化キーを渡す)
    bool initialize(const QString& dbPath, const QString& cipherKey);

    // コメントログの記録 (ユーザー名とメッセージは自動で暗号化される)
    bool logComment(const QString& userId, const QString& userName, const QString& message, double sentimentScore, bool isSpam);

    // 履歴の取得
    QList<CommentLog> getComments(const QDateTime& start = QDateTime(), const QDateTime& end = QDateTime());

    // 履歴の削除
    bool clearHistory(const QDateTime& start, const QDateTime& end);

private:
    QSqlDatabase m_db;
    QString m_cipherKey;

    bool createTables();
};
