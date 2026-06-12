#pragma once
#include <QEvent>
#include <QString>
#include <QStringList>
#include <QList>

namespace TwitchEvents {

struct ChatterInfo {
    QString userId;
    QString userName;
    QStringList userBadges;
};

// 独自のQEvent::Typeを登録 (1000以降を安全に利用するため registerEventType を使用)
// inlineだと翻訳単位ごとに異なるIDが振られる事故を防ぐため関数でくるむ
inline QEvent::Type commentReceivedType() {
    static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return type;
}

inline QEvent::Type chattersReceivedType() {
    static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return type;
}

class CommentEvent : public QEvent {
public:
    CommentEvent(const QString& userId, const QString& userName, const QString& message,
                 const QStringList& badges = QStringList(), const QStringList& badgeUrls = QStringList(),
                 const QString& messageId = QString())
        : QEvent(commentReceivedType())
        , m_userId(userId)
        , m_userName(userName)
        , m_message(message)
        , m_badges(badges)
        , m_badgeUrls(badgeUrls)
        , m_messageId(messageId)
    {}

    QString userId() const { return m_userId; }
    QString userName() const { return m_userName; }
    QString message() const { return m_message; }
    QStringList badges() const { return m_badges; }
    QStringList badgeUrls() const { return m_badgeUrls; }
    QString messageId() const { return m_messageId; }

private:
    QString m_userId;
    QString m_userName;
    QString m_message;
    QStringList m_badges;
    QStringList m_badgeUrls;
    QString m_messageId;
};

class ChattersEvent : public QEvent {
public:
    explicit ChattersEvent(const QList<ChatterInfo>& chatters)
        : QEvent(chattersReceivedType())
        , m_chatters(chatters)
    {}

    QList<ChatterInfo> chatters() const { return m_chatters; }

private:
    QList<ChatterInfo> m_chatters;
};

} // namespace TwitchEvents

