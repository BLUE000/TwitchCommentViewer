#include "AppController.h"
#include "modules/TwitchEventCollectorImpl.h"
#include "modules/BouyomiIntegrationImpl.h"
#include "events/TwitchEvents.h"
#include "MainWindow.h"
#include <QDebug>

AppController::AppController(QObject* parent) : QObject(parent) {
    m_configManager = std::make_unique<ConfigManager>(this);
    
    // 自身のインスタンスをイベントターゲットとして渡す
    m_twitchCollector = std::make_unique<TwitchEventCollectorImpl>(this);
    m_dbManager = std::make_unique<DatabaseManager>();
    m_bouyomiIntegration = std::make_unique<BouyomiIntegrationImpl>(this);
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
                // 認証情報をコレクターへ渡す
                auto* impl = dynamic_cast<TwitchEventCollectorImpl*>(m_twitchCollector.get());
                if (impl) {
                    impl->setAuthData(m_configManager->getClientId(), m_configManager->getAccessToken());
                }
                m_twitchCollector->connectToTwitch();
            } else {
                qWarning() << "OAuth Authentication Failed:" << errorMsg;
            }
        });
        
        // UIの認証ボタンが押されたらフローを開始する
        if (m_mainWindow) {
            connect(m_mainWindow, &MainWindow::authRequested, this, [this]() {
                qInfo() << "Auth requested from UI. Starting OAuth Flow...";
                m_configManager->startOAuthFlow();
            });
        }
    } else {
        qWarning() << "config.json could not be loaded. Missing Client ID.";
    }

    // 既にトークンが保存されていて有効なら自動接続するフロー
    if (!m_configManager->getAccessToken().isEmpty()) {
        auto* impl = dynamic_cast<TwitchEventCollectorImpl*>(m_twitchCollector.get());
        if (impl) {
            impl->setAuthData(m_configManager->getClientId(), m_configManager->getAccessToken());
        }
        m_twitchCollector->connectToTwitch();
    }
}

void AppController::customEvent(QEvent* event) {
    if (event->type() == TwitchEvents::commentReceivedType()) {
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
        
        // 棒読みちゃん連携 (スパム判定が入った場合はスキップするなど後で追加可能)
        if (!isSpam) {
            // 例: 「ユーザー名、メッセージ」の形式で読ませる
            m_bouyomiIntegration->sendText(commentEvent->userName() + "、" + commentEvent->message());
        }

        // UIへの反映処理
        if (m_mainWindow) {
            // QtのGUIスレッドから安全にUIを更新
            QMetaObject::invokeMethod(m_mainWindow, [this, name = commentEvent->userName(), msg = commentEvent->message()]() {
                m_mainWindow->addCommentToView(name, msg);
            }, Qt::QueuedConnection);
        }
    } else {
        QObject::customEvent(event);
    }
}
