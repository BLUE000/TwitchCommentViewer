#pragma once
#include <QEvent>
#include <QString>

namespace TwitchEvents {

// 独自のQEvent::Typeを登録 (1000以降を安全に利用するため registerEventType を使用)
const QEvent::Type CommentReceivedType = static_cast<QEvent::Type>(QEvent::registerEventType());

class CommentEvent : public QEvent {
public:
    CommentEvent(const QString& userId, const QString& userName, const QString& message)
        : QEvent(CommentReceivedType)
        , m_userId(userId)
        , m_userName(userName)
        , m_message(message)
    {}

    QString userId() const { return m_userId; }
    QString userName() const { return m_userName; }
    QString message() const { return m_message; }

private:
    QString m_userId;
    QString m_userName;
    QString m_message;
};

} // namespace TwitchEvents
