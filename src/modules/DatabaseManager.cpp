#include "DatabaseManager.h"
#include <QTimeZone>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <cipher_engine.h>

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
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

    // 配信セッション管理用テーブル
    QString createSessionsSql = 
        "CREATE TABLE IF NOT EXISTS stream_sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "start_time DATETIME NOT NULL, "
        "end_time DATETIME"
        ")";

    if (!query.exec(createSessionsSql)) {
        qWarning() << "Failed to create stream_sessions table:" << query.lastError().text();
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
    query.prepare("INSERT INTO chat_logs (timestamp, user_id, username_enc, message_enc, sentiment_score, is_spam) "
                  "VALUES (:timestamp, :userId, :userName, :message, :score, :spam)");
    
    query.bindValue(":timestamp", QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm:ss"));
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

QList<CommentLog> DatabaseManager::getComments(const QDateTime& start, const QDateTime& end) {
    QList<CommentLog> logs;
    QSqlQuery query(m_db);
    QString sql = "SELECT id, timestamp, user_id, username_enc FROM chat_logs WHERE 1=1";
    
    if (start.isValid()) {
        sql += " AND timestamp >= :start";
    }
    if (end.isValid()) {
        sql += " AND timestamp <= :end";
    }
    sql += " ORDER BY timestamp ASC";

    query.prepare(sql);
    if (start.isValid()) {
        query.bindValue(":start", start.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    }
    if (end.isValid()) {
        query.bindValue(":end", end.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    }

    if (!query.exec()) {
        qWarning() << "Failed to get comments:" << query.lastError().text();
        return logs;
    }

    while (query.next()) {
        CommentLog log;
        log.id = query.value(0).toLongLong();
        
        QString tsStr = query.value(1).toString();
        QDate date = QDate::fromString(tsStr.left(10), "yyyy-MM-dd");
        QTime time = QTime::fromString(tsStr.mid(11), "HH:mm:ss");
        log.timestamp = QDateTime(date, time, QTimeZone::utc());
        
        log.userId = query.value(2).toString();
        
        QByteArray encName = query.value(3).toByteArray();
        CipherResult decName = CipherEngine::decrypt(encName, m_cipherKey);
        if (decName.isSuccess()) {
            log.userName = QString::fromUtf8(decName.data());
        } else {
            log.userName = "Unknown";
        }
        logs.append(log);
    }

    return logs;
}

bool DatabaseManager::clearHistory(const QDateTime& start, const QDateTime& end) {
    QSqlQuery query(m_db);
    QString sql = "DELETE FROM chat_logs WHERE 1=1";
    
    if (start.isValid()) {
        sql += " AND timestamp >= :start";
    }
    if (end.isValid()) {
        sql += " AND timestamp <= :end";
    }

    query.prepare(sql);
    if (start.isValid()) {
        query.bindValue(":start", start.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    }
    if (end.isValid()) {
        query.bindValue(":end", end.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    }

    if (!query.exec()) {
        qWarning() << "Failed to clear history:" << query.lastError().text();
        return false;
    }
    return true;
}

qint64 DatabaseManager::createStreamSession(const QDateTime& startTime) {
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO stream_sessions (start_time) VALUES (:start_time)");
    query.bindValue(":start_time", startTime.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    if (!query.exec()) {
        qWarning() << "Failed to create stream session:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool DatabaseManager::closeStreamSession(qint64 sessionId, const QDateTime& endTime) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE stream_sessions SET end_time = :end_time WHERE id = :id");
    query.bindValue(":end_time", endTime.toUTC().toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":id", sessionId);
    if (!query.exec()) {
        qWarning() << "Failed to close stream session:" << query.lastError().text();
        return false;
    }
    return true;
}

qint64 DatabaseManager::findStreamSessionByStartTime(const QDateTime& startTime) {
    QSqlQuery query(m_db);
    QDateTime startUtc = startTime.toUTC();
    QDateTime minTime = startUtc.addSecs(-60);
    QDateTime maxTime = startUtc.addSecs(60);

    query.prepare("SELECT id FROM stream_sessions WHERE start_time >= :min_time AND start_time <= :max_time LIMIT 1");
    query.bindValue(":min_time", minTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":max_time", maxTime.toString("yyyy-MM-dd HH:mm:ss"));

    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return -1;
}

QList<StreamSession> DatabaseManager::getStreamSessions() {
    QList<StreamSession> sessions;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, start_time, end_time FROM stream_sessions ORDER BY start_time DESC")) {
        qWarning() << "Failed to get stream sessions:" << query.lastError().text();
        return sessions;
    }
    while (query.next()) {
        StreamSession session;
        session.id = query.value(0).toLongLong();
        
        QString startStr = query.value(1).toString();
        QDate startDate = QDate::fromString(startStr.left(10), "yyyy-MM-dd");
        QTime startTime = QTime::fromString(startStr.mid(11), "HH:mm:ss");
        session.startTime = QDateTime(startDate, startTime, QTimeZone::utc()).toLocalTime();
        
        if (!query.value(2).isNull()) {
            QString endStr = query.value(2).toString();
            QDate endDate = QDate::fromString(endStr.left(10), "yyyy-MM-dd");
            QTime endTime = QTime::fromString(endStr.mid(11), "HH:mm:ss");
            session.endTime = QDateTime(endDate, endTime, QTimeZone::utc()).toLocalTime();
        }
        sessions.append(session);
    }
    return sessions;
}
