#pragma once
#include <QObject>
#include <QEvent>
#include <memory>
#include "interfaces/ITwitchEventCollector.h"
#include "modules/DatabaseManager.h"
#include "modules/ConfigManager.h"
#include "interfaces/IBouyomiIntegration.h"

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
    std::unique_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<IBouyomiIntegration> m_bouyomiIntegration;
};
