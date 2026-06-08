#pragma once
#include <QObject>
#include <QEvent>
#include <memory>
#include "interfaces/ITwitchEventCollector.h"
#include "interfaces/IBouyomiIntegration.h"
#include "interfaces/ICommentAnalyzer.h"
#include "interfaces/IObsIntegration.h"
#include "events/TwitchEvents.h"
#include "modules/DatabaseManager.h"
#include "modules/ConfigManager.h"

class MainWindow;

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void initialize();
    void setMainWindow(MainWindow* mw) { m_mainWindow = mw; }

protected:
    // シグナルの代わりにEvent割り込みで処理を受け取る
    void customEvent(QEvent* event) override;

private:
    MainWindow* m_mainWindow = nullptr;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<ITwitchEventCollector> m_twitchCollector;
    std::unique_ptr<IBouyomiIntegration> m_bouyomiIntegration;
    std::unique_ptr<IObsIntegration> m_obsFileIntegration;
    std::unique_ptr<IObsIntegration> m_obsWsIntegration;
    std::unique_ptr<ICommentAnalyzer> m_commentAnalyzer;
    std::unique_ptr<DatabaseManager> m_dbManager;
    
    // UI通知用の中継メソッド（解析タブ用）
    void emitSpamDetected(const QString& username, const QString& reason, const QString& message);
    void emitTrendWordDetected(const QString& word, int count);
    void emitEmotionScored(const QString& username, const QString& message, double score);
};
