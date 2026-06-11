#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "modules/ConfigManager.h"

#include "modules/DatabaseManager.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QClipboard>
#include <QUrl>
#include <QDir>
#include <QProcess>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chatModel(new QStandardItemModel(0, 2, this))
{
    ui->setupUi(this);

    // ビューワタブの名称をコメントに変更
    ui->tabWidget->setTabText(0, "コメント");

    // 視聴者タブの追加
    m_tabChatters = new QWidget(this);
    ui->tabWidget->insertTab(1, m_tabChatters, "視聴者");
    setupChattersTab();

    // タブ変更シグナルの接続
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MainWindow::tabChanged);

    // QTabWidgetの右上にボタンを追加
    QWidget* cornerWidget = new QWidget(this);
    QHBoxLayout* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(5);
    
    QPushButton* btnReadme = new QPushButton("README", cornerWidget);
    QPushButton* btnAbout = new QPushButton("About", cornerWidget);
    cornerLayout->addWidget(btnReadme);
    cornerLayout->addWidget(btnAbout);
    ui->tabWidget->setCornerWidget(cornerWidget);

    connect(btnReadme, &QPushButton::clicked, this, &MainWindow::on_btnReadme_clicked);
    connect(btnAbout, &QPushButton::clicked, this, &MainWindow::on_btnAbout_clicked);

    // ボット除外設定のトグル
    connect(ui->btnToggleBot, &QPushButton::toggled, this, &MainWindow::on_btnToggleBot_toggled);
    
    // DB管理設定のトグル
    connect(ui->btnToggleDb, &QPushButton::toggled, this, &MainWindow::on_btnToggleDb_toggled);

    // 解析ログエリアのフォント設定
    ui->textBrowserAnalysis->setStyleSheet("font-family: Consolas, monospace; font-size: 10pt;");
    
    // 集計テーブルの初期設定
    ui->tableUserStats->setColumnCount(2);
    ui->tableUserStats->setHorizontalHeaderLabels({"User", "Comments"});
    ui->tableUserStats->horizontalHeader()->setStretchLastSection(true);

    setupChart();
    
    // Connect chart controls
    connect(ui->comboGraphType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onChartConfigChanged);
    connect(ui->comboGranularity, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onChartConfigChanged);
    connect(ui->comboTopN, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onChartConfigChanged);
    
    // Timer for chart
    m_chartUpdateTimer = new QTimer(this);
    m_chartUpdateTimer->setInterval(5000); // 5 seconds
    connect(m_chartUpdateTimer, &QTimer::timeout, this, &MainWindow::updateChartDisplay);
    m_chartUpdateTimer->start();

    // Bouyomi & OBS default UI states
    ui->groupBoxBouyomi->setVisible(true);
    ui->tableUserStats->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableUserStats->setSelectionBehavior(QAbstractItemView::SelectRows);

    // ビューワタブのテーブル設定
    m_chatModel->setHeaderData(0, Qt::Horizontal, "Username");
    m_chatModel->setHeaderData(1, Qt::Horizontal, "Message");
    ui->tableViewComments->setModel(m_chatModel);
    ui->tableViewComments->setColumnWidth(0, 150);
    ui->tableViewComments->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewComments->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewComments->setSelectionBehavior(QAbstractItemView::SelectRows);

    // ConfigManager が AppController から渡されるまでは空なので後で初期化される
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addCommentToView(const QString& username, const QString& message) {
    int newRow = m_chatModel->rowCount();
    m_chatModel->insertRow(newRow);
    m_chatModel->setData(m_chatModel->index(newRow, 0), username);
    m_chatModel->setData(m_chatModel->index(newRow, 1), message);
    
    // 最新コメントへ自動スクロール
    ui->tableViewComments->scrollToBottom();
}

void MainWindow::on_btnStartAuth_clicked() {
    emit authRequested();
}

void MainWindow::setConfigManager(ConfigManager* configManager) {
    m_configManager = configManager;
    
    // UIを初期化
    ui->editBouyomiHost->setText(m_configManager->bouyomiHost());
    ui->spinBouyomiPort->setValue(m_configManager->bouyomiPort());
    ui->editBouyomiExe->setText(m_configManager->bouyomiExePath());
    
    int voiceIdx = m_configManager->bouyomiVoice();
    if (voiceIdx >= 0 && voiceIdx < ui->comboBouyomiVoice->count()) {
        ui->comboBouyomiVoice->setCurrentIndex(voiceIdx);
    }
    
    ui->spinBouyomiVolume->setValue(m_configManager->bouyomiVolume());
    ui->chkBouyomiAutoStart->setChecked(m_configManager->getBouyomiAutoStart());
    ui->chkBouyomiAutoStop->setChecked(m_configManager->getBouyomiAutoStop());

    ui->chkObsFileOutput->setChecked(m_configManager->getObsFileOutputEnabled());
    ui->chkObsWebSocket->setChecked(m_configManager->getObsWebSocketEnabled());
    ui->spinObsPort->setValue(m_configManager->getObsServerPort());
    
    // ボットユーザーリスト
    ui->editBotUsers->setText(m_configManager->getBotUsers().join(", "));
    
    loadOverlayFiles();
    int idx = ui->comboObsOverlay->findText(m_configManager->getObsOverlayFile());
    if (idx >= 0) ui->comboObsOverlay->setCurrentIndex(idx);
        
    // OBSローカルのパスを生成してセットする (HTTP用)
    QString url = QString("http://localhost:%1/").arg(m_configManager->getObsServerPort());
    ui->editObsUrl->setText(url);
}

void MainWindow::on_btnBrowseBouyomi_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "棒読みちゃんの実行ファイルを選択", "", "実行ファイル (*.exe);;すべてのファイル (*.*)");
    if (!filePath.isEmpty()) {
        ui->editBouyomiExe->setText(filePath);
    }
}

void MainWindow::on_btnSaveBouyomi_clicked() {
    if (!m_configManager) return;
    
    m_configManager->setBouyomiHost(ui->editBouyomiHost->text());
    m_configManager->setBouyomiPort(ui->spinBouyomiPort->value());
    m_configManager->setBouyomiExePath(ui->editBouyomiExe->text());
    m_configManager->setBouyomiVoice(ui->comboBouyomiVoice->currentIndex());
    m_configManager->setBouyomiVolume(ui->spinBouyomiVolume->value());
    m_configManager->setBouyomiAutoStart(ui->chkBouyomiAutoStart->isChecked());
    m_configManager->setBouyomiAutoStop(ui->chkBouyomiAutoStop->isChecked());
    
    m_configManager->saveConfig();
    QMessageBox::information(this, "設定保存", "棒読みちゃんの設定を保存しました。");
}

void MainWindow::on_btnTestBouyomi_clicked() {
    QString testMessage = ui->editBouyomiTestText->text();
    if (testMessage.isEmpty()) {
        testMessage = "これはテストメッセージです";
    }
    emit bouyomiTestRequested(testMessage);
}

void MainWindow::appendAnalysisLog(const QString& logMsg) {
    ui->textBrowserAnalysis->append(logMsg);
}

void MainWindow::on_btnSaveObs_clicked() {
    if (m_configManager) {
        m_configManager->setObsFileOutputEnabled(ui->chkObsFileOutput->isChecked());
        m_configManager->setObsWebSocketEnabled(ui->chkObsWebSocket->isChecked());
        m_configManager->setObsServerPort(ui->spinObsPort->value());
        m_configManager->setObsOverlayFile(ui->comboObsOverlay->currentText());
        m_configManager->saveConfig();
        
        QString url = QString("http://localhost:%1/").arg(ui->spinObsPort->value());
        ui->editObsUrl->setText(url);
    }
}

void MainWindow::on_btnCopyObsUrl_clicked() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(ui->editObsUrl->text());
}

void MainWindow::on_btnToggleBouyomi_toggled(bool checked) {
    ui->groupBoxBouyomi->setVisible(checked);
    ui->btnToggleBouyomi->setText(checked ? "▼ 棒読みちゃん連携設定" : "▶ 棒読みちゃん連携設定");
}

void MainWindow::on_btnToggleObs_toggled(bool checked) {
    ui->groupBoxObs->setVisible(checked);
    ui->btnToggleObs->setText(checked ? "▼ OBS連携設定" : "▶ OBS連携設定");
}

void MainWindow::on_btnOpenOverlayFolder_clicked() {
    QString path = QCoreApplication::applicationDirPath() + "/assets/overlay";
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::on_btnToggleBot_toggled(bool checked) {
    ui->groupBoxBot->setVisible(checked);
    ui->btnToggleBot->setText(checked ? "▼ 解析・集計設定 (ボット除外)" : "▶ 解析・集計設定 (ボット除外)");
}

void MainWindow::on_btnSaveBot_clicked() {
    if (m_configManager) {
        QStringList botList;
        QString rawText = ui->editBotUsers->text();
        for (const QString& part : rawText.split(",")) {
            QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                botList.append(trimmed);
            }
        }
        m_configManager->setBotUsers(botList);
        m_configManager->saveConfig();
        emit botSettingsChanged(botList);
        QMessageBox::information(this, "設定保存", "解析・集計設定を保存しました。");
    }
}

void MainWindow::on_btnToggleDb_toggled(bool checked) {
    ui->groupBoxDb->setVisible(checked);
    ui->btnToggleDb->setText(checked ? "▼ データベース管理 (履歴表示)" : "▶ データベース管理 (履歴表示)");
}

void MainWindow::loadOverlayFiles() {
    ui->comboObsOverlay->clear();
    QString path = QCoreApplication::applicationDirPath() + "/assets/overlay";
    QDir dir(path);
    if (dir.exists()) {
        QStringList filters;
        filters << "*.html" << "*.htm";
        QStringList files = dir.entryList(filters, QDir::Files);
        ui->comboObsOverlay->addItems(files);
    }
    if (ui->comboObsOverlay->count() == 0) {
        ui->comboObsOverlay->addItem("overlay.html");
    }
}

void MainWindow::on_btnReadme_clicked() {
    QDesktopServices::openUrl(QUrl("https://github.com/BLUE000/TwitchCommentViewer/blob/master/README.md"));
}

void MainWindow::on_btnAbout_clicked() {
    QString versionStr = "1.0.0";
#ifdef APP_VERSION
    versionStr = QString(APP_VERSION);
#endif

    QString aboutText = 
        QString("<h2>TwitchCommentManager v%1</h2>").arg(versionStr) +
        "<p>Copyright (c) 2026 BLUE000</p>"
        "<h3>サードパーティ・ライセンス</h3>"
        "<p>本ソフトウェアは、以下のオープンソースライブラリおよびセキュリティモジュールを利用しています。</p>"
        "<p>This software uses TrustChain Module. Copyright (c) 2026 BLUE000.<br>"
        "Includes TransCipher, Copyright (c) 2026 BLUE000. (<a href=\"https://github.com/BLUE000/TransCipher-Dist\">https://github.com/BLUE000/TransCipher-Dist</a>)<br>"
        "Includes BinMarkManager, Copyright (c) 2026 BLUE. (<a href=\"https://github.com/BLUE000/BinMarkManager\">https://github.com/BLUE000/BinMarkManager</a>)</p>"
        "<p>Qt is licensed under the GNU Lesser General Public License (LGPL) version 3.<br>"
        "Copyright (C) 2024 The Qt Company Ltd and other contributors.<br>"
        "(<a href=\"https://www.qt.io/licensing/\">https://www.qt.io/licensing/</a>)</p>";

    QMessageBox::about(this, "About TwitchCommentManager", aboutText);
}


void MainWindow::updateStatistics(int totalComments, const QMap<QString, int>& userCounts, const QString& latestUser, const QDateTime& latestTime) {
    if (!latestUser.isEmpty() && latestTime.isValid()) {
        m_commentHistory.append({latestTime, latestUser});
        m_chartNeedsUpdate = true;
    }

    ui->labelTotalComments->setText(QString("総コメント数: %1").arg(totalComments));

    // ランキング順（降順）でソート
    QList<QPair<QString, int>> sortedList;
    for (auto it = userCounts.begin(); it != userCounts.end(); ++it) {
        sortedList.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sortedList.begin(), sortedList.end(), [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
        return a.second > b.second; // 降順
    });

    ui->tableUserStats->setRowCount(sortedList.size());
    for (int i = 0; i < sortedList.size(); ++i) {
        QTableWidgetItem* userItem = new QTableWidgetItem(sortedList[i].first);
        QTableWidgetItem* countItem = new QTableWidgetItem(QString::number(sortedList[i].second));
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        ui->tableUserStats->setItem(i, 0, userItem);
        ui->tableUserStats->setItem(i, 1, countItem);
    }
}

void MainWindow::setDatabaseManager(DatabaseManager* dbManager) {
    m_dbManager = dbManager;
}

void MainWindow::setupChart() {
    m_chart = new QChart();
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    
    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("時間");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    
    m_axisY = new QValueAxis();
    m_axisY->setLabelFormat("%d");
    m_axisY->setTitleText("コメント数");
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    
    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::loadHistoryFromDb() {
    if (!m_dbManager) return;
    
    m_commentHistory.clear();
    QList<CommentLog> logs = m_dbManager->getComments();
    
    int total = 0;
    QMap<QString, int> userCounts;
    
    for (const CommentLog& log : logs) {
        LogEntry entry;
        entry.time = log.timestamp;
        entry.user = log.userName;
        m_commentHistory.append(entry);
        
        total++;
        userCounts[log.userName]++;
    }
    
    updateStatistics(total, userCounts);
    m_chartNeedsUpdate = true;
    updateChartDisplay();
}

void MainWindow::onChartConfigChanged() {
    m_chartNeedsUpdate = true;
    updateChartDisplay();
}

void MainWindow::on_btnLaunchDbViewer_clicked() {
    QString exePath = QCoreApplication::applicationDirPath() + "/../tools/DBViewer/TwitchCommentDbViewer.exe";
    if (!QFile::exists(exePath)) {
        // Fallback for debug/release folders
        exePath = QCoreApplication::applicationDirPath() + "/TwitchCommentDbViewer.exe";
    }
    
    QString dbPath = QCoreApplication::applicationDirPath() + "/twitch_comments.db";
    
    QStringList args;
    args << dbPath;
    
    if (!QProcess::startDetached(exePath, args)) {
        QMessageBox::warning(this, "エラー", "データベースビューアの起動に失敗しました。\n" + exePath);
    }
}

void MainWindow::updateChartDisplay() {
    qDebug() << "updateChartDisplay start";
    if (!m_chartNeedsUpdate && m_commentHistory.isEmpty()) {
        qDebug() << "updateChartDisplay returning early (no update needed)";
        return;
    }
    m_chartNeedsUpdate = false;
    
    qDebug() << "removing series";
    m_chart->removeAllSeries();
    
    if (m_commentHistory.isEmpty()) {
        qDebug() << "updateChartDisplay returning early (history empty)";
        return;
    }
    
    qDebug() << "calculating bounds";
    qint64 firstTime = m_commentHistory.first().time.toMSecsSinceEpoch();
    qint64 lastTime = m_commentHistory.last().time.toMSecsSinceEpoch();
    qint64 durationMs = lastTime - firstTime;
    if (durationMs < 5000) durationMs = 5000;
    
    // 粒度決定
    qint64 binSizeMs = 5000; // default 5s
    int granIdx = ui->comboGranularity->currentIndex();
    if (granIdx == 0) { // 自動
        binSizeMs = qMax(5000LL, durationMs / 60);
    } else if (granIdx == 1) binSizeMs = 5000;
    else if (granIdx == 2) binSizeMs = 10000;
    else if (granIdx == 3) binSizeMs = 30000;
    else if (granIdx == 4) binSizeMs = 60000;
    else if (granIdx == 5) binSizeMs = 300000;
    
    int typeIdx = ui->comboGraphType->currentIndex();
    int topNIdx = ui->comboTopN->currentIndex();
    
    QMap<QString, int> userTotals;
    for (const auto& entry : m_commentHistory) {
        userTotals[entry.user]++;
    }
    int uniqueUsers = userTotals.keys().size();
    
    // パフォーマンス保護: 100人超で全員表示の場合はTOP10にフォールバック
    if (uniqueUsers > 100 && topNIdx == 3) {
        topNIdx = 2; // TOP 10
        ui->comboTopN->setCurrentIndex(2);
    }
    
    QStringList targetUsers;
    if (typeIdx == 1) { // ユーザー別
        QList<QPair<QString, int>> sortedUsers;
        for (auto it = userTotals.begin(); it != userTotals.end(); ++it) {
            sortedUsers.append(qMakePair(it.key(), it.value()));
        }
        std::sort(sortedUsers.begin(), sortedUsers.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
        
        int limit = sortedUsers.size();
        if (topNIdx == 0) limit = qMin(limit, 3);
        else if (topNIdx == 1) limit = qMin(limit, 5);
        else if (topNIdx == 2) limit = qMin(limit, 10);
        // topNIdx == 3 is all
        
        for (int i = 0; i < limit; ++i) {
            targetUsers.append(sortedUsers[i].first);
        }
    }
    
    // データをビンに集計
    QMap<QString, QMap<qint64, int>> binnedData; // user -> (binTime -> count)
    // "TOTAL" key for total graph
    qint64 maxCount = 0;
    
    for (const auto& entry : m_commentHistory) {
        qint64 t = entry.time.toMSecsSinceEpoch();
        qint64 binTime = ((t - firstTime) / binSizeMs) * binSizeMs + firstTime;
        
        if (typeIdx == 0) {
            binnedData["TOTAL"][binTime]++;
        } else {
            if (targetUsers.contains(entry.user)) {
                binnedData[entry.user][binTime]++;
            }
        }
    }
    
    // シリーズ作成
    for (auto it = binnedData.begin(); it != binnedData.end(); ++it) {
        QLineSeries* series = new QLineSeries();
        series->setName(it.key() == "TOTAL" ? "総コメント数" : it.key());
        
        auto bins = it.value();
        // 最初の時間から最後の時間まで0埋めしつつプロット
        for (qint64 t = firstTime; t <= lastTime + binSizeMs; t += binSizeMs) {
            int count = bins.value(t, 0);
            series->append(t, count);
            if (count > maxCount) maxCount = count;
        }
        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
    }
    
    qDebug() << "setting axis ranges";
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(firstTime), QDateTime::fromMSecsSinceEpoch(lastTime + binSizeMs));
    m_axisY->setRange(0, maxCount + 1);
    m_axisY->setTickCount(qMin(10LL, maxCount + 2));
    qDebug() << "updateChartDisplay finish";
}

void MainWindow::setupChattersTab() {
    auto* mainLayout = new QVBoxLayout(m_tabChatters);

    // 上部コントロール
    auto* topLayout = new QHBoxLayout();
    m_btnUpdateChatters = new QPushButton("更新", m_tabChatters);
    m_btnUpdateChatters->setEnabled(false); // 初期状態は無効
    m_labelLastUpdate = new QLabel("最終更新: 未取得", m_tabChatters);
    
    topLayout->addWidget(m_btnUpdateChatters);
    topLayout->addWidget(m_labelLastUpdate);
    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // スクロールエリア
    auto* scrollArea = new QScrollArea(m_tabChatters);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto* scrollWidget = new QWidget(scrollArea);
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(5, 5, 5, 5);
    scrollLayout->setSpacing(5);

    // アコーディオン作成マクロ的にヘルパーラムダを使用
    auto createAccordion = [this, scrollWidget, scrollLayout](QPushButton*& btn, QListWidget*& list, const QString& title) {
        btn = new QPushButton("▶ " + title + " (0)", scrollWidget);
        btn->setStyleSheet("text-align: left; font-weight: bold; padding: 5px;");
        list = new QListWidget(scrollWidget);
        list->setVisible(false); // 初期状態は折りたたみ
        list->setSelectionMode(QAbstractItemView::NoSelection);
        
        scrollLayout->addWidget(btn);
        scrollLayout->addWidget(list);
        
        connect(btn, &QPushButton::clicked, this, [btn, list, title]() {
            bool visible = !list->isVisible();
            list->setVisible(visible);
            btn->setText(QString("%1 %2 (%3)")
                .arg(visible ? "▼" : "▶")
                .arg(title)
                .arg(list->count()));
        });
    };

    createAccordion(m_btnToggleAll, m_listAll, "すべて");
    createAccordion(m_btnToggleBroadcaster, m_listBroadcaster, "ストリーマー");
    createAccordion(m_btnToggleModerator, m_listModerator, "モデレーター");
    createAccordion(m_btnToggleVip, m_listVip, "VIP");
    createAccordion(m_btnToggleBot, m_listBot, "チャットボット");
    createAccordion(m_btnToggleRegular, m_listRegular, "一般");

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 更新ボタンのシグナル接続
    connect(m_btnUpdateChatters, &QPushButton::clicked, this, &MainWindow::chatterRefreshRequested);
}

void MainWindow::setUpdateButtonEnabled(bool enabled) {
    if (m_btnUpdateChatters) {
        m_btnUpdateChatters->setEnabled(enabled);
    }
}

void MainWindow::updateChattersList(const QList<QPair<QString, QString>>& chatters) {
    if (!m_listAll) return;

    m_listAll->clear();
    m_listBroadcaster->clear();
    m_listModerator->clear();
    m_listVip->clear();
    m_listBot->clear();
    m_listRegular->clear();

    QStringList botUsers = m_configManager ? m_configManager->getBotUsers() : QStringList();
    QStringList lowerBots;
    for (const QString& b : botUsers) {
        lowerBots.append(b.toLower());
    }

    for (const auto& pair : chatters) {
        QString name = pair.first;
        QString role = pair.second;

        m_listAll->addItem(name);

        if (lowerBots.contains(name.toLower())) {
            m_listBot->addItem(name);
        } else if (role == "broadcaster") {
            m_listBroadcaster->addItem(name);
        } else if (role == "moderator") {
            m_listModerator->addItem(name);
        } else if (role == "vip") {
            m_listVip->addItem(name);
        } else {
            m_listRegular->addItem(name);
        }
    }

    // 各アコーディオンボタンのテキスト更新
    auto updateBtnText = [](QPushButton* btn, QListWidget* list, const QString& title) {
        btn->setText(QString("%1 %2 (%3)")
            .arg(list->isVisible() ? "▼" : "▶")
            .arg(title)
            .arg(list->count()));
    };

    updateBtnText(m_btnToggleAll, m_listAll, "すべて");
    updateBtnText(m_btnToggleBroadcaster, m_listBroadcaster, "ストリーマー");
    updateBtnText(m_btnToggleModerator, m_listModerator, "モデレーター");
    updateBtnText(m_btnToggleVip, m_listVip, "VIP");
    updateBtnText(m_btnToggleBot, m_listBot, "チャットボット");
    updateBtnText(m_btnToggleRegular, m_listRegular, "一般");

    m_labelLastUpdate->setText("最終更新: " + QDateTime::currentDateTime().toString("HH:mm:ss"));
}
