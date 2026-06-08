#pragma once
#include <QObject>
#include <QEvent>
#include <memory>
#include "interfaces/ITwitchEventCollector.h"
#include "modules/DatabaseManager.h"

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void initialize();

protected:
    // シグナルの代わりにEvent割り込みで処理を受け取る
    void customEvent(QEvent* event) override;

private:
    std::unique_ptr<ITwitchEventCollector> m_twitchCollector;
    std::unique_ptr<DatabaseManager> m_dbManager;
};
