#include "AppController.h"
#include "modules/TwitchEventCollectorImpl.h"
#include "modules/BouyomiIntegrationImpl.h"
#include "modules/VoiceVoxIntegrationImpl.h"
#include "modules/ObsFileIntegrationImpl.h"
#include "modules/ObsWebSocketServer.h"
#include "modules/ObsHttpServer.h"
#include "modules/CommentAnalyzer.h"
#include "events/TwitchEvents.h"
#include "MainWindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
// アバター画像処理用ヘルパー
static QPixmap getCircularPixmap(const QPixmap& src) {
    QPixmap target(src.size());
    target.fill(Qt::transparent);
    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    path.addEllipse(0, 0, src.width(), src.height());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, src);
    return target;
}

static QIcon getPlaceholderIcon() {
    static QIcon placeholder;
    if (placeholder.isNull()) {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor(180, 180, 180));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(0, 0, 32, 32);
        placeholder = QIcon(pix);
    }
    return placeholder;
}


AppController::AppController(QObject* parent) : QObject(parent) {
    m_configManager = std::make_unique<ConfigManager>(this);
    
    // 自身のインスタンスをイベントターゲットとして渡す
    m_twitchCollector = std::make_unique<TwitchEventCollectorImpl>(this);
    m_ttsIntegration = nullptr;
    
    // OBS連携モジュールの初期化 (ポートは初期化時に設定)
    m_obsFileIntegration = std::make_unique<ObsFileIntegrationImpl>(this);
    m_obsWsIntegration = nullptr; // 後でポートを取得してから初期化
    m_obsHttpServer = std::make_unique<ObsHttpServer>(this);
    
    // DatabaseManager のスレッド分離と非同期初期化
    m_dbThread = std::make_unique<QThread>(this);
    m_dbManager = std::make_unique<DatabaseManager>(); // 親なしで作成
    m_dbManager->moveToThread(m_dbThread.get());
    m_dbThread->start();

    // CommentAnalyzer のスレッド分離
    m_analyzerThread = std::make_unique<QThread>(this);
    auto analyzer = new CommentAnalyzer(); // 親なしで作成
    analyzer->setBotUsers(m_configManager->getBotUsers());
    analyzer->moveToThread(m_analyzerThread.get());
    
    connect(analyzer, &CommentAnalyzer::spamDetected,
            this, &AppController::emitSpamDetected);
    connect(analyzer, &CommentAnalyzer::trendWordDetected,
            this, &AppController::emitTrendWordDetected);
    connect(analyzer, &CommentAnalyzer::emotionScored,
            this, &AppController::emitEmotionScored);
    connect(analyzer, &CommentAnalyzer::statisticsUpdated,
            this, &AppController::emitStatisticsUpdated);
            
    m_analyzerThread->start();
    m_commentAnalyzer.reset(analyzer);
}

AppController::~AppController() {
    m_twitchCollector->disconnectFromTwitch();
    
    if (m_analyzerThread) {
        m_analyzerThread->quit();
        m_analyzerThread->wait();
    }
    if (m_dbThread) {
        m_dbThread->quit();
        m_dbThread->wait();
    }
}

void AppController::initialize() {
    // DBの初期化 (QThread上で非同期実行)
    QString dbPath = QCoreApplication::applicationDirPath() + "/twitch_comments.db";
    QMetaObject::invokeMethod(m_dbManager.get(), "initialize", Qt::QueuedConnection,
                              Q_ARG(QString, dbPath), Q_ARG(QString, "application_runtime_key"));

    // 設定を読み込み（ロードの成否に関わらず、一部の値は読み込まれている可能性がある）
    bool configLoaded = m_configManager->loadConfig();
    
    reloadTtsIntegration();

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

    if (configLoaded && !m_configManager->getClientId().isEmpty()) {
        connect(m_configManager.get(), &ConfigManager::authCompleted, this, [this](bool success, const QString& errorMsg) {
            if (success) {
                qInfo() << "OAuth Flow Successful. Proceeding to connect Twitch EventSub...";
                // 認証情報をコレクターへ渡す
                auto* impl = dynamic_cast<TwitchEventCollectorImpl*>(m_twitchCollector.get());
                if (impl) {
                    impl->setAuthData(m_configManager->getClientId(), m_configManager->getAccessToken());
                }
                m_twitchCollector->connectToTwitch();

                if (m_mainWindow) {
                    m_mainWindow->showStatusMessage("Twitchとの連携認証に成功しました！自動接続を開始します。");
                }
            } else {
                qWarning() << "OAuth Authentication Failed:" << errorMsg;
                if (m_mainWindow) {
                    QMessageBox::critical(m_mainWindow, "認証失敗",
                        "Twitchとの連携認証に失敗しました。\n"
                        "エラー: " + errorMsg);
                }
            }
        });
    } else {
        qWarning() << "config.json could not be loaded or twitch_client_id is empty.";
    }
    
    // UIの初期設定 (設定ロードの成否に関わらず必ず行う)
    if (m_mainWindow) {
        m_mainWindow->setConfigManager(m_configManager.get());
        m_mainWindow->setController(this);
        
        // MainWindowへDB参照を渡し、履歴をロード
        m_mainWindow->setDatabaseManager(m_dbManager.get());
        m_mainWindow->loadHistoryFromDb();
    }

    // 既にトークンが保存されていて有効なら自動接続するフロー
    if (configLoaded && !m_configManager->getClientId().isEmpty() && !m_configManager->getAccessToken().isEmpty()) {
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
            
            // 解析を非同期でスレッドへ委譲
            QObject* analyzerObj = dynamic_cast<QObject*>(m_commentAnalyzer.get());
            QMetaObject::invokeMethod(analyzerObj, "analyzeComment", Qt::QueuedConnection,
                                      Q_ARG(QString, commentEvent->userId()),
                                      Q_ARG(QString, commentEvent->userName()),
                                      Q_ARG(QString, safeMessage));
        }

        bool isSpam = false;
        double sentiment = 0.0;

        // DBへのセキュア保存を非同期でスレッドへ委譲
        QMetaObject::invokeMethod(m_dbManager.get(), "logComment", Qt::QueuedConnection,
                                  Q_ARG(QString, commentEvent->userId()),
                                  Q_ARG(QString, commentEvent->userName()),
                                  Q_ARG(QString, safeMessage),
                                  Q_ARG(double, sentiment),
                                  Q_ARG(bool, isSpam));
        
        // 棒読みちゃん連携 (スパム判定や除外設定ユーザーのコメントはスキップ)
        if (!isSpam) {
            bool ttsIgnored = false;
            if (m_configManager) {
                QString userNameLower = commentEvent->userName().toLower();
                
                // 1. 読み上げ除外ユーザーのチェック（これに該当する場合のみ読み上げを除外）
                QStringList ignoreList = m_configManager->getTtsIgnoreUsers();
                for (const QString& u : ignoreList) {
                    if (u.trimmed().compare(userNameLower, Qt::CaseInsensitive) == 0) {
                        ttsIgnored = true;
                        break;
                    }
                }
            }
            
            if (!ttsIgnored && m_ttsIntegration) {
                // 例: 「ユーザー名、メッセージ」の形式で読ませる
                m_ttsIntegration->sendText(commentEvent->userName() + "、" + safeMessage);
            }
            
            // OBSアバター連携 (解析・集計設定の「ボットリスト」に含まれるユーザーはアバター表示から除外)
            bool obsIgnored = false;
            if (m_configManager) {
                QString userNameLower = commentEvent->userName().toLower();
                QStringList botList = m_configManager->getBotUsers();
                for (const QString& b : botList) {
                    if (b.trimmed().compare(userNameLower, Qt::CaseInsensitive) == 0) {
                        obsIgnored = true;
                        break;
                    }
                }
            }
            
            if (!obsIgnored) {
                // OBS連携 (ONに設定されているもののみ実行)
                QVariantMap obsPayload;
                obsPayload["username"] = commentEvent->userName();
                obsPayload["message"] = safeMessage;
                
                QString userId = commentEvent->userId();
                if (m_avatarUrlCache.contains(userId)) {
                    obsPayload["avatarUrl"] = m_avatarUrlCache[userId];
                }
                obsPayload["badges"] = commentEvent->badgeUrls();
                
                if (m_configManager->getObsFileOutputEnabled() && m_obsFileIntegration) {
                    m_obsFileIntegration->sendAction("comment", obsPayload);
                }
                if (m_configManager->getObsWebSocketEnabled() && m_obsWsIntegration) {
                    m_obsWsIntegration->sendAction("comment", obsPayload);
                }
            }
        }

        QString userId = commentEvent->userId();

        // アーティストバッジを自動検出して永続キャッシュに記録
        if (commentEvent->badges().contains("artist")) {
            if (m_configManager) {
                m_configManager->addArtist(userId);
            }
        }

        // アバター画像の非同期取得・キャッシュ制御
        QIcon avatarIcon;
        if (m_avatarCache.contains(userId)) {
            avatarIcon = m_avatarCache[userId];
        } else {
            avatarIcon = getPlaceholderIcon();
            if (!m_pendingAvatars.contains(userId)) {
                m_pendingAvatars.insert(userId);
                fetchAvatar(userId);
            }
        }

        // UIへの反映処理
        if (m_mainWindow) {
            QString messageId = commentEvent->messageId();
            // QtのGUIスレッドから安全にUIを更新
            QMetaObject::invokeMethod(m_mainWindow, [this, userId, name = commentEvent->userName(), msg = safeMessage, avatarIcon, badges = commentEvent->badges(), messageId]() {
                m_mainWindow->addCommentToView(userId, name, msg, avatarIcon, badges, messageId);
                m_mainWindow->addChatterFromComment(userId, name, badges);
            }, Qt::QueuedConnection);
        }
    } else if (event->type() == TwitchEvents::chattersReceivedType()) {
        auto* chattersEvent = static_cast<TwitchEvents::ChattersEvent*>(event);
        m_lastChattersFetchTime = QDateTime::currentDateTime();
        
        // タイマーコールバックを即時呼んでボタンの無効化（クールダウン開始）を反映
        onChattersTimerTimeout();
        
        if (m_mainWindow) {
            auto list = chattersEvent->chatters();
            if (m_configManager) {
                for (auto& info : list) {
                    if (m_configManager->isArtist(info.userId)) {
                        if (!info.userBadges.contains("artist")) {
                            info.userBadges.append("artist");
                        }
                    }
                }
            }
            QMetaObject::invokeMethod(m_mainWindow, [this, list]() {
                if (m_mainWindow) m_mainWindow->updateChattersList(list);
            }, Qt::QueuedConnection);
        }
    } else if (event->type() == TwitchEvents::streamOnlineType()) {
        auto* onlineEvent = static_cast<TwitchEvents::StreamOnlineEvent*>(event);
        QDateTime startedAt = onlineEvent->startedAt();
        qInfo() << "AppController: Stream online event received, started at" << startedAt;

        // DBスレッドで非同期検索＆作成を行う
        qint64 activeId = -1;
        QMetaObject::invokeMethod(m_dbManager.get(), [this, startedAt, &activeId]() {
            activeId = m_dbManager->findStreamSessionByStartTime(startedAt);
            if (activeId == -1) {
                activeId = m_dbManager->createStreamSession(startedAt);
            }
        }, Qt::BlockingQueuedConnection);

        m_activeSessionId = activeId;
        qInfo() << "AppController: Active stream session ID set to:" << m_activeSessionId;

        // UIへ通知
        if (m_mainWindow) {
            QMetaObject::invokeMethod(m_mainWindow, [this]() {
                if (m_mainWindow) {
                    m_mainWindow->handleStreamStatusChanged(true, m_activeSessionId);
                }
            }, Qt::QueuedConnection);
        }
    } else if (event->type() == TwitchEvents::streamOfflineType()) {
        qInfo() << "AppController: Stream offline event received.";

        if (m_activeSessionId != -1) {
            qint64 sessionId = m_activeSessionId;
            QDateTime endTime = QDateTime::currentDateTime();
            QMetaObject::invokeMethod(m_dbManager.get(), [this, sessionId, endTime]() {
                m_dbManager->closeStreamSession(sessionId, endTime);
            }, Qt::QueuedConnection);

            m_activeSessionId = -1;
        }

        // UIへ通知
        if (m_mainWindow) {
            QMetaObject::invokeMethod(m_mainWindow, [this]() {
                if (m_mainWindow) {
                    m_mainWindow->handleStreamStatusChanged(false, -1);
                }
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

void AppController::updateBotUsers(const QStringList& bots) {
    if (m_commentAnalyzer) {
        QObject* analyzerObj = dynamic_cast<QObject*>(m_commentAnalyzer.get());
        if (analyzerObj) {
            QMetaObject::invokeMethod(analyzerObj, "setBotUsers", Qt::QueuedConnection,
                                      Q_ARG(QStringList, bots));
        }
    }
}

void AppController::emitStatisticsUpdated(int totalComments, const QMap<QString, int>& userCounts, const QString& latestUser, const QDateTime& latestTime) {
    if (m_mainWindow) {
        QMetaObject::invokeMethod(m_mainWindow, [this, totalComments, userCounts, latestUser, latestTime]() {
            if (m_mainWindow) m_mainWindow->updateStatistics(totalComments, userCounts, latestUser, latestTime);
        }, Qt::QueuedConnection);
    }
}

void AppController::onTabWidgetChanged(int index) {
    // インデックス 1 が「視聴者」タブ
    if (index == 1) {
        m_chattersTabActive = true;
        qInfo() << "Viewer tab activated.";

        // 初回表示時、または10分（600秒）以上経過していれば取得
        if (m_lastChattersFetchTime.isNull() || 
            m_lastChattersFetchTime.secsTo(QDateTime::currentDateTime()) >= 600) {
            triggerChattersFetch();
        }

        if (m_chattersTimer == nullptr) {
            m_chattersTimer = new QTimer(this);
            m_chattersTimer->setInterval(1000); // 1秒ごとにチェック
            connect(m_chattersTimer, &QTimer::timeout, this, &AppController::onChattersTimerTimeout);
        }
        m_chattersTimer->start();
        onChattersTimerTimeout(); // ボタンの初期状態反映
    } else {
        if (m_chattersTabActive) {
            m_chattersTabActive = false;
            qInfo() << "Viewer tab deactivated. Stopping polling timer.";
            if (m_chattersTimer) {
                m_chattersTimer->stop();
            }
        }
    }
}

void AppController::onChattersTimerTimeout() {
    if (!m_chattersTabActive || !m_mainWindow) return;

    if (m_lastChattersFetchTime.isNull()) {
        m_mainWindow->setUpdateButtonEnabled(true);
    } else {
        qint64 secs = m_lastChattersFetchTime.secsTo(QDateTime::currentDateTime());
        
        // 3分（180秒）経過していれば手動更新可能
        m_mainWindow->setUpdateButtonEnabled(secs >= 180);

        // 10分（600秒）経過していれば自動再取得
        if (secs >= 600) {
            qInfo() << "10 minutes passed since last fetch. Auto refetching chatters...";
            triggerChattersFetch();
        }
    }
}

void AppController::triggerChattersFetch() {
    if (m_twitchCollector) {
        // ボタンを先行して無効化（二重取得防止）
        if (m_mainWindow) {
            m_mainWindow->setUpdateButtonEnabled(false);
        }
        m_twitchCollector->requestChatters();
    }
}

void AppController::onChatterShoutoutRequested(const QString& targetUserId) {
    if (m_twitchCollector) {
        m_twitchCollector->sendShoutout(targetUserId);
    }
}

void AppController::onChatterVipToggled(const QString& targetUserId, bool enable) {
    if (m_twitchCollector) {
        m_twitchCollector->setVipStatus(targetUserId, enable);
    }
}

void AppController::onChatterModeratorToggled(const QString& targetUserId, bool enable) {
    if (m_twitchCollector) {
        m_twitchCollector->setModeratorStatus(targetUserId, enable);
    }
}

void AppController::onChatterTimeoutRequested(const QString& targetUserId, int duration) {
    if (m_twitchCollector) {
        m_twitchCollector->banUser(targetUserId, duration);
    }
}

void AppController::onChatterBanToggled(const QString& targetUserId, bool enable) {
    if (m_twitchCollector) {
        if (enable) {
            m_twitchCollector->banUser(targetUserId, 0); // 0 is perm ban
        } else {
            m_twitchCollector->unbanUser(targetUserId);
        }
    }
}

void AppController::fetchAvatar(const QString& userId) {
    if (!m_configManager) return;
    QString clientId = m_configManager->getClientId();
    QString token = m_configManager->getAccessToken();
    if (clientId.isEmpty() || token.isEmpty()) return;

    if (!m_nam) {
        m_nam = std::make_unique<QNetworkAccessManager>(this);
    }

    QUrl url("https://api.twitch.tv/helix/users?id=" + userId);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    request.setRawHeader("Client-Id", clientId.toUtf8());

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isNull() && doc.isObject()) {
                QJsonArray data = doc.object()["data"].toArray();
                if (!data.isEmpty()) {
                    QString imgUrl = data.first().toObject()["profile_image_url"].toString();
                    if (!imgUrl.isEmpty()) {
                        m_avatarUrlCache[userId] = imgUrl;
                        downloadAvatarImage(userId, imgUrl);
                        
                        // アバター画像URLが非同期で取得できたことをOBS側にWebSocketでリアルタイム通知
                        QVariantMap updatePayload;
                        updatePayload["userId"] = userId;
                        updatePayload["avatarUrl"] = imgUrl;
                        if (m_configManager && m_configManager->getObsWebSocketEnabled() && m_obsWsIntegration) {
                            m_obsWsIntegration->sendAction("update_avatar", updatePayload);
                        }

                        reply->deleteLater();
                        return; // successfully went to download
                    }
                }
            }
        } else {
            qWarning() << "Failed to fetch user profile for avatar:" << reply->errorString();
        }
        m_pendingAvatars.remove(userId);
        reply->deleteLater();
    });
}

void AppController::downloadAvatarImage(const QString& userId, const QString& urlStr) {
    if (!m_nam) return;
    QNetworkRequest request = QNetworkRequest(QUrl(urlStr));
    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userId]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                QPixmap circular = getCircularPixmap(pixmap);
                QIcon icon(circular);
                
                m_avatarCache[userId] = icon;
                m_pendingAvatars.remove(userId);

                // UIの既存行を更新
                if (m_mainWindow) {
                    QMetaObject::invokeMethod(m_mainWindow, [this, userId, icon]() {
                        m_mainWindow->updateAvatarIcon(userId, icon);
                    }, Qt::QueuedConnection);
                }
            } else {
                m_pendingAvatars.remove(userId);
            }
        } else {
            qWarning() << "Failed to download avatar image:" << reply->errorString();
            m_pendingAvatars.remove(userId);
        }
        reply->deleteLater();
    });
}

void AppController::startOAuthFlow() {
    if (m_configManager) {
        m_configManager->startOAuthFlow();
    }
}

void AppController::ttsTestRequested(const QString& message) {
    if (m_ttsIntegration) {
        m_ttsIntegration->sendText(message);
    }
}

void AppController::onPinCommentRequested(const QString& messageId, int durationSeconds) {
    if (m_twitchCollector) {
        m_twitchCollector->pinChatMessage(messageId, durationSeconds);
    }
}

void AppController::onUnpinCommentRequested(const QString& messageId) {
    if (m_twitchCollector) {
        m_twitchCollector->unpinChatMessage(messageId);
    }
}

void AppController::onSendAnnouncementRequested(const QString& message, const QString& color) {
    if (m_twitchCollector) {
        m_twitchCollector->sendAnnouncement(message, color);
    }
}

void AppController::reloadTtsIntegration() {
    qInfo() << "Reloading TTS integration (active engine:" << (m_configManager ? m_configManager->activeTtsEngine() : 0) << ")...";
    m_ttsIntegration.reset(); // 旧モジュールの破棄（自動停止含む）

    if (!m_configManager) return;
    int engine = m_configManager->activeTtsEngine();
    if (engine == 1) {
        m_ttsIntegration = std::make_unique<BouyomiIntegrationImpl>(m_configManager.get(), this);
    } else if (engine == 2) {
        m_ttsIntegration = std::make_unique<VoiceVoxIntegrationImpl>(m_configManager.get(), this);
    }

    if (m_ttsIntegration) {
        m_ttsIntegration->initialize();
    }
}

void AppController::onBroadcastObsActionRequested(const QString& actionType, const QVariantMap& payload) {
    if (m_obsWsIntegration) {
        m_obsWsIntegration->sendAction(actionType, payload);
    }
}

void AppController::reloadObsServer() {
    if (!m_configManager) return;

    int httpPort = m_configManager->getObsServerPort();

    if (m_obsHttpServer) {
        m_obsHttpServer->setIndexFile(m_configManager->getObsOverlayFile());

        if (m_obsHttpServer->serverPort() != httpPort || !m_obsHttpServer->isListening()) {
            m_obsHttpServer->close();
            if (m_obsHttpServer->listen(QHostAddress::Any, httpPort)) {
                qInfo() << "OBS HTTP Server re-started and listening on port" << httpPort;
            } else {
                qWarning() << "Failed to restart OBS HTTP Server on port" << httpPort;
            }
        }
    }

    if (m_configManager->getObsWebSocketEnabled()) {
        if (!m_obsWsIntegration || m_obsWsIntegration->serverPort() != (httpPort + 1)) {
            m_obsWsIntegration.reset();
            m_obsWsIntegration = std::make_unique<ObsWebSocketServer>(httpPort + 1, this);
            qInfo() << "OBS WebSocket Server re-started on port" << (httpPort + 1);
        }
    } else {
        if (m_obsWsIntegration) {
            m_obsWsIntegration.reset();
            qInfo() << "OBS WebSocket Server stopped";
        }
    }
}

