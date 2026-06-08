#include "DatabaseManager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <cipher_engine.h>

DatabaseManager::DatabaseManager() {
}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::initialize(const QString& dbPath, const QString& cipherKey) {
    m_cipherKey = cipherKey;
    m_db = QSqlDatabase::addDatabase("QSQLITE", "TwitchDBConnection");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_db);
    // 詳細設計書の「3.2 データベーススキーマ設計」に従う
    QString createSql = 
        "CREATE TABLE IF NOT EXISTS chat_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
        "user_id TEXT, "
        "username_enc BLOB, "
        "message_enc BLOB, "
        "sentiment_score REAL, "
        "is_spam INTEGER"
        ")";

    if (!query.exec(createSql)) {
        qWarning() << "Failed to create chat_logs table:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::logComment(const QString& userId, const QString& userName, const QString& message, double sentimentScore, bool isSpam) {
    // 1. TransCipherを利用したプライバシー情報の難読化(暗号化)
    CipherResult encName = CipherEngine::encrypt(userName.toUtf8(), m_cipherKey);
    CipherResult encMsg = CipherEngine::encrypt(message.toUtf8(), m_cipherKey);

    if (!encName.isSuccess() || !encMsg.isSuccess()) {
        qWarning() << "Failed to encrypt data before DB insertion. Aborting insert.";
        return false;
    }

    // 2. SQLiteへのセキュアな書き込み
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO chat_logs (user_id, username_enc, message_enc, sentiment_score, is_spam) "
                  "VALUES (:userId, :userName, :message, :score, :spam)");
    
    query.bindValue(":userId", userId);
    query.bindValue(":userName", encName.data());
    query.bindValue(":message", encMsg.data());
    query.bindValue(":score", sentimentScore);
    query.bindValue(":spam", isSpam ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Failed to insert chat log:" << query.lastError().text();
        return false;
    }
    return true;
}
