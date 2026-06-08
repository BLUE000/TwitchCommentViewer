#pragma once
#include <QEvent>
#include <QString>

namespace TwitchEvents {

// 独自のQEvent::Typeを登録 (1000以降を安全に利用するため registerEventType を使用)
// inlineだと翻訳単位ごとに異なるIDが振られる事故を防ぐため関数でくるむ
inline QEvent::Type commentReceivedType() {
    static QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return type;
}

class CommentEvent : public QEvent {
public:
    CommentEvent(const QString& userId, const QString& userName, const QString& message)
        : QEvent(commentReceivedType())
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
