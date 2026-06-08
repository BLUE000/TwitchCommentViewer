#include "AppController.h"
#include "modules/TwitchEventCollectorImpl.h"
#include "modules/BouyomiIntegrationImpl.h"
#include "modules/ObsFileIntegrationImpl.h"
#include "modules/ObsWebSocketServer.h"
#include "modules/ObsHttpServer.h"
#include "modules/CommentAnalyzer.h"
#include "events/TwitchEvents.h"
#include "MainWindow.h"
#include <QDebug>

AppController::AppController(QObject* parent) : QObject(parent) {
    m_configManager = std::make_unique<ConfigManager>(this);
    
    // 自身のインスタンスをイベントターゲットとして渡す
    m_twitchCollector = std::make_unique<TwitchEventCollectorImpl>(this);
    m_dbManager = std::make_unique<DatabaseManager>();
    m_bouyomiIntegration = std::make_unique<BouyomiIntegrationImpl>(m_configManager.get(), this);
    
    // OBS連携モジュールの初期化 (ポートは初期化時に設定)
    m_obsFileIntegration = std::make_unique<ObsFileIntegrationImpl>(this);
    m_obsWsIntegration = nullptr; // 後でポートを取得してから初期化
    m_obsHttpServer = std::make_unique<ObsHttpServer>(this);
    
    // コメントアナライザーの初期化とシグナル接続
    auto analyzer = std::make_unique<CommentAnalyzer>(this);
    connect(analyzer.get(), &CommentAnalyzer::spamDetected, this, &AppController::emitSpamDetected);
    connect(analyzer.get(), &CommentAnalyzer::trendWordDetected, this, &AppController::emitTrendWordDetected);
    connect(analyzer.get(), &CommentAnalyzer::emotionScored, this, &AppController::emitEmotionScored);
    m_commentAnalyzer = std::move(analyzer);
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
        if (m_bouyomiIntegration) {
            m_bouyomiIntegration->initialize();
        }

        // HTTPサーバーとWebSocketサーバーの起動
        if (m_obsHttpServer) {
            int httpPort = m_configManager->getObsServerPort();
            m_obsHttpServer->setIndexFile(m_configManager->getObsOverlayFile());
            if (m_obsHttpServer->listen(QHostAddress::Any, httpPort)) {
                qInfo() << "OBS HTTP Server listening on port" << httpPort;
            } else {
                qWarning() << "Failed to start OBS HTTP Server on port" << httpPort;
            }

            if (!m_obsWsIntegration) {
                m_obsWsIntegration = std::make_unique<ObsWebSocketServer>(httpPort + 1, this);
            }
        }

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
            m_mainWindow->setConfigManager(m_configManager.get());
            
            connect(m_mainWindow, &MainWindow::authRequested, this, [this]() {
                qInfo() << "Auth requested from UI. Starting OAuth Flow...";
                m_configManager->startOAuthFlow();
            });

            connect(m_mainWindow, &MainWindow::bouyomiTestRequested, this, [this](const QString& message) {
                qInfo() << "Bouyomi test requested:" << message;
                if (m_bouyomiIntegration) {
                    m_bouyomiIntegration->sendText(message);
                }
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

        // 解析実行とサニタイズ（伏せ字化）
        QString safeMessage = commentEvent->message();
        if (m_commentAnalyzer) {
            safeMessage = m_commentAnalyzer->sanitizeMessage(commentEvent->message());
            m_commentAnalyzer->analyzeComment(commentEvent->userId(), commentEvent->userName(), safeMessage);
        }

        bool isSpam = false;
        double sentiment = 0.0;

        // DBへのセキュア保存
        m_dbManager->logComment(commentEvent->userId(), 
                                commentEvent->userName(), 
                                safeMessage, 
                                sentiment, isSpam);
        
        // 棒読みちゃん連携 (スパム判定が入った場合はスキップするなど後で追加可能)
        if (!isSpam) {
            // 例: 「ユーザー名、メッセージ」の形式で読ませる
            m_bouyomiIntegration->sendText(commentEvent->userName() + "、" + safeMessage);
            
            // OBS連携 (ONに設定されているもののみ実行)
            QVariantMap obsPayload;
            obsPayload["username"] = commentEvent->userName();
            obsPayload["message"] = safeMessage;
            
            if (m_configManager->getObsFileOutputEnabled() && m_obsFileIntegration) {
                m_obsFileIntegration->sendAction("comment", obsPayload);
            }
            if (m_configManager->getObsWebSocketEnabled() && m_obsWsIntegration) {
                m_obsWsIntegration->sendAction("comment", obsPayload);
            }
        }

        // UIへの反映処理
        if (m_mainWindow) {
            // QtのGUIスレッドから安全にUIを更新
            QMetaObject::invokeMethod(m_mainWindow, [this, name = commentEvent->userName(), msg = safeMessage]() {
                m_mainWindow->addCommentToView(name, msg);
            }, Qt::QueuedConnection);
        }
    } else {
        QObject::customEvent(event);
    }
}

void AppController::emitSpamDetected(const QString& username, const QString& reason, const QString& message) {
    if (m_mainWindow) {
        QMetaObject::invokeMethod(m_mainWindow, "appendAnalysisLog", Qt::QueuedConnection,
                                  Q_ARG(QString, QString("[Spam] %1: %2 (%3)").arg(username, reason, message)));
    }
}

void AppController::emitTrendWordDetected(const QString& word, int count) {
    if (m_mainWindow) {
        QMetaObject::invokeMethod(m_mainWindow, "appendAnalysisLog", Qt::QueuedConnection,
                                  Q_ARG(QString, QString("[Trend] '%1' is trending! (Count: %2)").arg(word).arg(count)));
    }
}

void AppController::emitEmotionScored(const QString& username, const QString& message, double score) {
    if (m_mainWindow) {
        QString emotion = score > 0 ? "Positive" : "Negative";
        QMetaObject::invokeMethod(m_mainWindow, "appendAnalysisLog", Qt::QueuedConnection,
                                  Q_ARG(QString, QString("[%1: %2] %3: %4").arg(emotion).arg(score, 0, 'f', 2).arg(username, message)));
    }
}
