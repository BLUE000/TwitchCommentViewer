#include "AppController.h"
#include "modules/TwitchEventCollectorImpl.h"
#include "events/TwitchEvents.h"
#include <QDebug>

AppController::AppController(QObject* parent) : QObject(parent) {
    m_configManager = std::make_unique<ConfigManager>(this);
    
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

    // 設定を読み込み、OAuth認証を開始。成功したらTwitchへ接続。
    if (m_configManager->loadConfig()) {
        connect(m_configManager.get(), &ConfigManager::authCompleted, this, [this](bool success, const QString& errorMsg) {
            if (success) {
                qInfo() << "OAuth Flow Successful. Proceeding to connect Twitch EventSub...";
                m_twitchCollector->connectToTwitch();
            } else {
                qWarning() << "OAuth Authentication Failed:" << errorMsg;
            }
        });
        
        // 認証フロー開始（必要ならブラウザが開く）
        m_configManager->startOAuthFlow();
    } else {
        qWarning() << "config.json could not be loaded. Missing Client ID.";
    }
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
