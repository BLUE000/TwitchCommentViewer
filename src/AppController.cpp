#include "AppController.h"
#include "modules/TwitchEventCollectorImpl.h"
#include "events/TwitchEvents.h"
#include <QDebug>

AppController::AppController(QObject* parent) : QObject(parent) {
    // 自身のインスタンスをイベントターゲットとして渡す
    m_twitchCollector = std::make_unique<TwitchEventCollectorImpl>(this);
    m_dbManager = std::make_unique<DatabaseManager>();
}

AppController::~AppController() {
    m_twitchCollector->disconnectFromTwitch();
}

void AppController::initialize() {
    // DBの初期化 (本来は設定から鍵を取得)
    if (!m_dbManager->initialize("twitch_comments.db", "application_runtime_key")) {
        qWarning() << "Failed to initialize DBManager in AppController";
    }

    // Twitch WebSocket 接続開始
    m_twitchCollector->connectToTwitch();
}

void AppController::customEvent(QEvent* event) {
    if (event->type() == TwitchEvents::CommentReceivedType) {
        auto* commentEvent = static_cast<TwitchEvents::CommentEvent*>(event);
        
        qInfo() << "AppController received Event Interrupt!" 
                << "\n User:" << commentEvent->userName() 
                << "\n Msg:" << commentEvent->message();

        // TODO: CommentAnalyzer によるスパム判定と感情分析
        bool isSpam = false;
        double sentiment = 0.0;

        // DBへのセキュア保存
        m_dbManager->logComment(commentEvent->userId(), 
                                commentEvent->userName(), 
                                commentEvent->message(), 
                                sentiment, isSpam);
        
        // TODO: UIへの反映処理など
    } else {
        QObject::customEvent(event);
    }
}
