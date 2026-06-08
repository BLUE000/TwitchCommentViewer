#pragma once
#include <QString>
#include <QSqlDatabase>

// UT-DB-01 および要件定義に基づくSQLiteとTransCipher連携DB管理
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // データベース初期化 (DBファイルパスとTransCipher用暗号化キーを渡す)
    bool initialize(const QString& dbPath, const QString& cipherKey);

    // コメントログの記録 (ユーザー名とメッセージは自動で暗号化される)
    bool logComment(const QString& userId, const QString& userName, const QString& message, double sentimentScore, bool isSpam);

private:
    QSqlDatabase m_db;
    QString m_cipherKey;

    bool createTables();
};
