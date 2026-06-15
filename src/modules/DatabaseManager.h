#pragma once
#include <QObject>
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

struct StreamSession {
    qint64 id;
    QDateTime startTime;
    QDateTime endTime;
};

// UT-DB-01 および要件定義に基づくSQLiteとTransCipher連携DB管理
class DatabaseManager : public QObject {
    Q_OBJECT
public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

public slots:
    // データベース初期化 (DBファイルパスとTransCipher用暗号化キーを渡す)
    bool initialize(const QString& dbPath, const QString& cipherKey);

    // コメントログの記録 (ユーザー名とメッセージは自動で暗号化される)
    bool logComment(const QString& userId, const QString& userName, const QString& message, double sentimentScore, bool isSpam);

    // 履歴の取得
    QList<CommentLog> getComments(const QDateTime& start = QDateTime(), const QDateTime& end = QDateTime());

    // 履歴の削除
    bool clearHistory(const QDateTime& start, const QDateTime& end);

    // 配信セッションの操作
    qint64 createStreamSession(const QDateTime& startTime);
    bool closeStreamSession(qint64 sessionId, const QDateTime& endTime);
    qint64 findStreamSessionByStartTime(const QDateTime& startTime);
    QList<StreamSession> getStreamSessions();

private:
    QSqlDatabase m_db;
    QString m_cipherKey;

    bool createTables();
};
