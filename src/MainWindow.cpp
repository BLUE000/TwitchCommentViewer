#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "modules/ConfigManager.h"
#include <QMenu>
#include <QAction>
#include <QResizeEvent>

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
#include "modules/ChatterRowWidget.h"
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chatModel(new QStandardItemModel(0, 2, this))
{
    ui->setupUi(this);

    // ビューワタブの名称をコメントに変更
    ui->tabWidget->setTabText(0, "コメント");

    // 視聴者タブのセットアップ
    m_tabChatters = ui->tabChatters;
    setupChattersTab();

    // タブ変更イベントを非同期でコントローラへ通知
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "onTabWidgetChanged", Qt::QueuedConnection, Q_ARG(int, index));
        }
    });

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
    ui->groupBoxTts->setVisible(true);
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
    ui->tableViewComments->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableViewComments, &QTableView::customContextMenuRequested, this, &MainWindow::showCommentContextMenu);

    // 読み上げ設定の変更検知接続
    connect(ui->radUseBouyomi, &QRadioButton::toggled, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->radUseVoicevox, &QRadioButton::toggled, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->spinTtsVolume, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->doubleSpinTtsSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->doubleSpinTtsPitch, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->chkTtsAutoStart, &QCheckBox::toggled, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->chkTtsAutoStop, &QCheckBox::toggled, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->editTtsIgnoreUsers, &QLineEdit::textChanged, this, [this](){ markTtsSettingsUnsaved(true); });

    connect(ui->editBouyomiHost, &QLineEdit::textChanged, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->spinBouyomiPort, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->editBouyomiExe, &QLineEdit::textChanged, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->comboBouyomiVoice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](){ markTtsSettingsUnsaved(true); });

    connect(ui->editVoicevoxHost, &QLineEdit::textChanged, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->spinVoicevoxPort, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->editVoicevoxExe, &QLineEdit::textChanged, this, [this](){ markTtsSettingsUnsaved(true); });
    connect(ui->spinVoicevoxSpeaker, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](){ markTtsSettingsUnsaved(true); });

    // ConfigManager が AppController から渡されるまでは空なので後で初期化される
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addCommentToView(const QString& userId, const QString& username, const QString& message, const QIcon& icon, const QStringList& badges, const QString& messageId) {
    auto getBadgeEmoji = [](const QString& badge) -> QString {
        if (badge == "broadcaster") return "👑";
        if (badge == "moderator") return "🛡️";
        if (badge == "vip") return "💎";
        if (badge == "subscriber") return "⭐";
        if (badge == "artist") return "🎨";
        return "";
    };

    QString displayName = username;
    if (!badges.isEmpty()) {
        QString badgeStr;
        for (const QString& b : badges) {
            QString emoji = getBadgeEmoji(b);
            if (!emoji.isEmpty()) {
                badgeStr += emoji;
            }
        }
        if (!badgeStr.isEmpty()) {
            displayName += " " + badgeStr;
        }
    }

    auto* userItem = new QStandardItem(displayName);
    userItem->setIcon(icon);
    userItem->setData(userId, Qt::UserRole);

    auto* msgItem = new QStandardItem(message);
    msgItem->setData(messageId, Qt::UserRole + 1);

    m_chatModel->appendRow({userItem, msgItem});
    
    // 最新コメントへ自動スクロール
    ui->tableViewComments->scrollToBottom();
}

void MainWindow::updateAvatarIcon(const QString& userId, const QIcon& icon) {
    for (int i = 0; i < m_chatModel->rowCount(); ++i) {
        QModelIndex idx = m_chatModel->index(i, 0);
        if (m_chatModel->data(idx, Qt::UserRole).toString() == userId) {
            if (auto* item = m_chatModel->item(i, 0)) {
                item->setIcon(icon);
            }
        }
    }
}

void MainWindow::setController(QObject* controller) {
    m_controller = controller;
}

void MainWindow::on_btnStartAuth_clicked() {
    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "startOAuthFlow", Qt::QueuedConnection);
    }
}

void MainWindow::setConfigManager(ConfigManager* configManager) {
    m_configManager = configManager;
    
    // UIを初期化
    int engine = m_configManager->activeTtsEngine();
    
    // 先にシグナルが走って値が上書きされるのを防ぐため、一時的にシグナルをブロックする
    ui->radUseBouyomi->blockSignals(true);
    ui->radUseVoicevox->blockSignals(true);

    if (engine == 2) {
        ui->radUseVoicevox->setChecked(true);
        ui->stackedTtsEngines->setCurrentIndex(1);
    } else {
        ui->radUseBouyomi->setChecked(true);
        ui->stackedTtsEngines->setCurrentIndex(0);
    }

    ui->radUseBouyomi->blockSignals(false);
    ui->radUseVoicevox->blockSignals(false);

    // 棒読みちゃん個別設定
    ui->editBouyomiHost->setText(m_configManager->bouyomiHost());
    ui->spinBouyomiPort->setValue(m_configManager->bouyomiPort());
    ui->editBouyomiExe->setText(m_configManager->bouyomiExePath());
    int voiceIdx = m_configManager->bouyomiVoice();
    if (voiceIdx >= 0 && voiceIdx < ui->comboBouyomiVoice->count()) {
        ui->comboBouyomiVoice->setCurrentIndex(voiceIdx);
    }

    // VOICEVOX個別設定
    ui->editVoicevoxHost->setText(m_configManager->voicevoxHost());
    ui->spinVoicevoxPort->setValue(m_configManager->voicevoxPort());
    ui->editVoicevoxExe->setText(m_configManager->voicevoxExePath());
    ui->spinVoicevoxSpeaker->setValue(m_configManager->voicevoxSpeaker());

    // 共通コントロールに現在のアクティブエンジンの設定値を適用
    if (engine == 2) {
        ui->spinTtsVolume->setMinimum(0);
        ui->spinTtsVolume->setMaximum(100);
        ui->spinTtsVolume->setValue(m_configManager->voicevoxVolume());
        
        ui->doubleSpinTtsSpeed->setMinimum(0.5);
        ui->doubleSpinTtsSpeed->setMaximum(2.0);
        ui->doubleSpinTtsSpeed->setSingleStep(0.1);
        ui->doubleSpinTtsSpeed->setValue(m_configManager->voicevoxSpeed());
        ui->labelTtsSpeedUnit->setText("倍");

        ui->doubleSpinTtsPitch->setMinimum(-0.15);
        ui->doubleSpinTtsPitch->setMaximum(0.15);
        ui->doubleSpinTtsPitch->setSingleStep(0.01);
        ui->doubleSpinTtsPitch->setValue(m_configManager->voicevoxPitch());
        ui->labelTtsPitchUnit->setText("");
    } else {
        ui->spinTtsVolume->setMinimum(-1);
        ui->spinTtsVolume->setMaximum(100);
        ui->spinTtsVolume->setValue(m_configManager->bouyomiVolume());
        
        ui->doubleSpinTtsSpeed->setMinimum(50);
        ui->doubleSpinTtsSpeed->setMaximum(300);
        ui->doubleSpinTtsSpeed->setSingleStep(10);
        ui->doubleSpinTtsSpeed->setValue(m_configManager->bouyomiSpeed());
        ui->labelTtsSpeedUnit->setText("%");

        ui->doubleSpinTtsPitch->setMinimum(50);
        ui->doubleSpinTtsPitch->setMaximum(200);
        ui->doubleSpinTtsPitch->setSingleStep(10);
        ui->doubleSpinTtsPitch->setValue(m_configManager->bouyomiPitch());
        ui->labelTtsPitchUnit->setText("%");
    }

    // 共通チェックボックス・除外ユーザーなど
    ui->chkTtsAutoStart->setChecked(m_configManager->getTtsAutoStart());
    ui->chkTtsAutoStop->setChecked(m_configManager->getTtsAutoStop());
    ui->editTtsIgnoreUsers->setText(m_configManager->getTtsIgnoreUsers().join(", "));

    ui->chkObsFileOutput->setChecked(m_configManager->getObsFileOutputEnabled());
    ui->chkObsWebSocket->setChecked(m_configManager->getObsWebSocketEnabled());
    ui->spinObsPort->setValue(m_configManager->getObsServerPort());
    
    // ボットユーザーリスト
    ui->editBotUsers->setText(m_configManager->getBotUsers().join(", "));
    
    loadOverlayFiles();
    int idx = ui->comboObsOverlay->findText(m_configManager->getObsOverlayFile());
    if (idx >= 0) ui->comboObsOverlay->setCurrentIndex(idx);

    ui->spinObsAvatarMinSize->setValue(m_configManager->getObsAvatarMinSize());
    ui->spinObsAvatarMaxSize->setValue(m_configManager->getObsAvatarMaxSize());
    ui->spinObsBounceFactor->setValue(m_configManager->getObsBounceFactor());
    ui->spinObsBrowserWidth->setValue(m_configManager->getObsBrowserWidth());
    ui->spinObsBrowserHeight->setValue(m_configManager->getObsBrowserHeight());
    ui->editObsEffectSymbols->setText(m_configManager->getObsEffectSymbols());
    ui->spinObsEffectSize->setValue(m_configManager->getObsEffectSize());
    ui->spinObsEffectCount->setValue(m_configManager->getObsEffectCount());

    updateObsPhysicsPreview();
    on_comboObsOverlay_currentTextChanged(m_configManager->getObsOverlayFile());
        
    // OBSローカルのパスを生成してセットする (HTTP用)
    QString url = QString("http://localhost:%1/").arg(m_configManager->getObsServerPort());
    ui->editObsUrl->setText(url);

    markTtsSettingsUnsaved(false);
}

void MainWindow::on_btnBrowseBouyomi_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "棒読みちゃんの実行ファイルを選択", "", "実行ファイル (*.exe);;すべてのファイル (*.*)");
    if (!filePath.isEmpty()) {
        ui->editBouyomiExe->setText(filePath);
    }
}

void MainWindow::on_btnBrowseVoicevox_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "VOICEVOXの実行ファイルを選択", "", "実行ファイル (*.exe);;すべてのファイル (*.*)");
    if (!filePath.isEmpty()) {
        ui->editVoicevoxExe->setText(filePath);
    }
}

void MainWindow::on_btnSaveTts_clicked() {
    if (!m_configManager) return;
    
    int engine = ui->radUseVoicevox->isChecked() ? 2 : 1;
    m_configManager->setActiveTtsEngine(engine);

    // 共通ボリューム・速度・音程の退避
    if (engine == 2) {
        m_configManager->setVoicevoxVolume(ui->spinTtsVolume->value());
        m_configManager->setVoicevoxSpeed(ui->doubleSpinTtsSpeed->value());
        m_configManager->setVoicevoxPitch(ui->doubleSpinTtsPitch->value());
    } else {
        m_configManager->setBouyomiVolume(ui->spinTtsVolume->value());
        m_configManager->setBouyomiSpeed(static_cast<int>(ui->doubleSpinTtsSpeed->value()));
        m_configManager->setBouyomiPitch(static_cast<int>(ui->doubleSpinTtsPitch->value()));
    }

    // 棒読みちゃん個別設定の保存
    m_configManager->setBouyomiHost(ui->editBouyomiHost->text());
    m_configManager->setBouyomiPort(ui->spinBouyomiPort->value());
    m_configManager->setBouyomiExePath(ui->editBouyomiExe->text());
    m_configManager->setBouyomiVoice(ui->comboBouyomiVoice->currentIndex());

    // VOICEVOX個別設定の保存
    m_configManager->setVoicevoxHost(ui->editVoicevoxHost->text());
    m_configManager->setVoicevoxPort(ui->spinVoicevoxPort->value());
    m_configManager->setVoicevoxExePath(ui->editVoicevoxExe->text());
    m_configManager->setVoicevoxSpeaker(ui->spinVoicevoxSpeaker->value());

    // 共通自動起動・自動終了
    m_configManager->setTtsAutoStart(ui->chkTtsAutoStart->isChecked());
    m_configManager->setTtsAutoStop(ui->chkTtsAutoStop->isChecked());
    
    QStringList ignoreList;
    QString rawText = ui->editTtsIgnoreUsers->text();
    for (const QString& part : rawText.split(",")) {
        QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            ignoreList.append(trimmed);
        }
    }
    m_configManager->setTtsIgnoreUsers(ignoreList);

    m_configManager->saveConfig();
    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "reloadTtsIntegration", Qt::QueuedConnection);
    }
    markTtsSettingsUnsaved(false);
    showStatusMessage("読み上げ設定を保存しました。", 3000);
}

void MainWindow::on_btnTestTts_clicked() {
    QString testMessage = ui->editTtsTestText->text();
    if (testMessage.isEmpty()) {
        testMessage = "これはテストメッセージです";
    }
    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "ttsTestRequested", Qt::QueuedConnection, Q_ARG(QString, testMessage));
    }
}

void MainWindow::appendAnalysisLog(const QString& logMsg) {
    ui->textBrowserAnalysis->append(logMsg);
}

void MainWindow::showStatusMessage(const QString& message, int timeoutMs) {
    if (ui->statusbar) {
        ui->statusbar->showMessage(message, timeoutMs);
    }
}

void MainWindow::on_btnSaveObs_clicked() {
    if (m_configManager) {
        m_configManager->setObsFileOutputEnabled(ui->chkObsFileOutput->isChecked());
        m_configManager->setObsWebSocketEnabled(ui->chkObsWebSocket->isChecked());
        m_configManager->setObsServerPort(ui->spinObsPort->value());
        m_configManager->setObsOverlayFile(ui->comboObsOverlay->currentText());
        
        m_configManager->setObsAvatarMinSize(ui->spinObsAvatarMinSize->value());
        m_configManager->setObsAvatarMaxSize(ui->spinObsAvatarMaxSize->value());
        m_configManager->setObsBounceFactor(ui->spinObsBounceFactor->value());
        m_configManager->setObsBrowserWidth(ui->spinObsBrowserWidth->value());
        m_configManager->setObsBrowserHeight(ui->spinObsBrowserHeight->value());
        m_configManager->setObsEffectSymbols(ui->editObsEffectSymbols->text());
        m_configManager->setObsEffectSize(ui->spinObsEffectSize->value());
        m_configManager->setObsEffectCount(ui->spinObsEffectCount->value());
        
        m_configManager->saveConfig();

        // サーバーの設定を即座に適用
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "reloadObsServer", Qt::QueuedConnection);
        }
        
        QString url = QString("http://localhost:%1/").arg(ui->spinObsPort->value());
        ui->editObsUrl->setText(url);
        showStatusMessage("OBS連携の設定を保存しました。", 3000);
    }
}

void MainWindow::on_btnCopyObsUrl_clicked() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(ui->editObsUrl->text());
}

void MainWindow::on_btnToggleTts_toggled(bool checked) {
    ui->groupBoxTts->setVisible(checked);
    ui->btnToggleTts->setText(checked ? "▼ 読み上げ設定" : "▶ 読み上げ設定");
}

void MainWindow::on_radUseBouyomi_toggled(bool checked) {
    if (!checked) return;
    ui->stackedTtsEngines->setCurrentIndex(0);

    // VOICEVOX側の設定をUIからConfigManagerに退避
    if (m_configManager) {
        m_configManager->setVoicevoxVolume(ui->spinTtsVolume->value());
        m_configManager->setVoicevoxSpeed(ui->doubleSpinTtsSpeed->value());
        m_configManager->setVoicevoxPitch(ui->doubleSpinTtsPitch->value());
    }

    // UIの表示範囲と値を棒読みちゃん用に切り替え
    ui->spinTtsVolume->setMinimum(-1);
    ui->spinTtsVolume->setMaximum(100);
    ui->doubleSpinTtsSpeed->setMinimum(50);
    ui->doubleSpinTtsSpeed->setMaximum(300);
    ui->doubleSpinTtsSpeed->setSingleStep(10);
    ui->labelTtsSpeedUnit->setText("%");

    ui->doubleSpinTtsPitch->setMinimum(50);
    ui->doubleSpinTtsPitch->setMaximum(200);
    ui->doubleSpinTtsPitch->setSingleStep(10);
    ui->labelTtsPitchUnit->setText("%");

    if (m_configManager) {
        ui->spinTtsVolume->setValue(m_configManager->bouyomiVolume());
        ui->doubleSpinTtsSpeed->setValue(m_configManager->bouyomiSpeed());
        ui->doubleSpinTtsPitch->setValue(m_configManager->bouyomiPitch());
    }

    markTtsSettingsUnsaved(true);
}

void MainWindow::on_radUseVoicevox_toggled(bool checked) {
    if (!checked) return;
    ui->stackedTtsEngines->setCurrentIndex(1);

    // 棒読みちゃん側の設定をUIからConfigManagerに退避
    if (m_configManager) {
        m_configManager->setBouyomiVolume(ui->spinTtsVolume->value());
        m_configManager->setBouyomiSpeed(static_cast<int>(ui->doubleSpinTtsSpeed->value()));
        m_configManager->setBouyomiPitch(static_cast<int>(ui->doubleSpinTtsPitch->value()));
    }

    // UIの表示範囲と値をVOICEVOX用に切り替え
    ui->spinTtsVolume->setMinimum(0);
    ui->spinTtsVolume->setMaximum(100);
    ui->doubleSpinTtsSpeed->setMinimum(0.5);
    ui->doubleSpinTtsSpeed->setMaximum(2.0);
    ui->doubleSpinTtsSpeed->setSingleStep(0.1);
    ui->labelTtsSpeedUnit->setText("倍");

    ui->doubleSpinTtsPitch->setMinimum(-0.15);
    ui->doubleSpinTtsPitch->setMaximum(0.15);
    ui->doubleSpinTtsPitch->setSingleStep(0.01);
    ui->labelTtsPitchUnit->setText("");

    if (m_configManager) {
        ui->spinTtsVolume->setValue(m_configManager->voicevoxVolume());
        ui->doubleSpinTtsSpeed->setValue(m_configManager->voicevoxSpeed());
        ui->doubleSpinTtsPitch->setValue(m_configManager->voicevoxPitch());
    }

    markTtsSettingsUnsaved(true);
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
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "updateBotUsers", Qt::QueuedConnection, Q_ARG(QStringList, botList));
        }
        showStatusMessage("解析・集計設定を保存しました。", 3000);
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
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subdir : subdirs) {
            QDir subDirObj(dir.filePath(subdir));
            if (subDirObj.exists("index.html")) {
                ui->comboObsOverlay->addItem(subdir + "/index.html");
            }
        }
    }
    if (ui->comboObsOverlay->count() == 0) {
        ui->comboObsOverlay->addItem("standard/index.html");
    }
}

void MainWindow::on_btnReadme_clicked() {
    QDesktopServices::openUrl(QUrl("https://github.com/BLUE000/TwitchCommentViewer/blob/master/README.md"));
}

void MainWindow::on_btnAbout_clicked() {
    QString versionStr = "1.4.0";
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
    m_axisX->setTickCount(5); // 横軸の目盛り数を5にして重複・省略を防ぐ
    
    QFont labelFont = m_axisX->labelsFont();
    labelFont.setPointSize(8); // フォントサイズを少し小さくして表示スペースを確保
    m_axisX->setLabelsFont(labelFont);
    
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    
    m_axisY = new QValueAxis();
    m_axisY->setLabelFormat("%d");
    m_axisY->setTitleText("コメント数");
    m_axisY->setLabelsFont(labelFont);
    
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    
    ui->chartView->setChart(m_chart);
    ui->chartView->setRenderHint(QPainter::Antialiasing);

    // 配信セッションのコントロール作成
    QLabel* labelSession = new QLabel("対象配信:", this);
    m_comboStreamSession = new QComboBox(this);
    m_comboStreamSession->setObjectName("comboStreamSession");
    m_comboStreamSession->setMinimumWidth(180);
    
    // レイアウトへの追加（先頭）
    ui->horizontalLayout_stats_ctrl->insertWidget(0, labelSession);
    ui->horizontalLayout_stats_ctrl->insertWidget(1, m_comboStreamSession);
    
    connect(m_comboStreamSession, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onStreamSessionChanged);
}

void MainWindow::loadHistoryFromDb() {
    if (!m_dbManager) return;
    
    refreshSessionsComboBox();
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

    // 横軸（X軸）の時間間隔をキリのいい数字に揃える処理
    QList<qint64> niceStepsX = {
        5000LL,       // 5s
        10000LL,      // 10s
        30000LL,      // 30s
        60000LL,      // 1m
        120000LL,     // 2m
        300000LL,     // 5m
        600000LL,     // 10m
        900000LL,     // 15m
        1800000LL,    // 30m
        3600000LL,    // 1h
        7200000LL,    // 2h
        10800000LL,   // 3h
        21600000LL,   // 6h
        43200000LL,   // 12h
        86400000LL    // 24h
    };
    int chartWidth = ui->chartView->width();
    qint64 targetDivisionsX = qMax(2LL, static_cast<qint64>(chartWidth / 150));
    qint64 targetIntervalX = durationMs / targetDivisionsX;
    qint64 niceIntervalX = 5000LL;
    for (qint64 step : niceStepsX) {
        if (step >= targetIntervalX) {
            niceIntervalX = step;
            break;
        }
    }
    // 間隔は最低でもビンサイズ以上にする
    if (niceIntervalX < binSizeMs) {
        niceIntervalX = binSizeMs;
    }

    qint64 firstTimeRounded = (firstTime / niceIntervalX) * niceIntervalX;
    qint64 lastTimeWithBin = lastTime + binSizeMs;
    qint64 lastTimeRounded = ((lastTimeWithBin + niceIntervalX - 1) / niceIntervalX) * niceIntervalX;
    if (lastTimeRounded <= firstTimeRounded) {
        lastTimeRounded = firstTimeRounded + niceIntervalX;
    }
    qint64 tickCountX = ((lastTimeRounded - firstTimeRounded) / niceIntervalX) + 1;
    
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
    
    // データをビンに集計（1970年からの絶対時間で基準化）
    QMap<QString, QMap<qint64, int>> binnedData; // user -> (binTime -> count)
    // "TOTAL" key for total graph
    qint64 maxCount = 0;
    
    for (const auto& entry : m_commentHistory) {
        qint64 t = entry.time.toMSecsSinceEpoch();
        qint64 binTime = firstTimeRounded + ((t - firstTimeRounded) / binSizeMs) * binSizeMs;
        
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
        for (qint64 t = firstTimeRounded; t <= lastTimeRounded; t += binSizeMs) {
            int count = bins.value(t, 0);
            series->append(t, count);
            if (count > maxCount) maxCount = count;
        }
        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
    }
    
    qDebug() << "setting axis ranges";
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(firstTimeRounded), QDateTime::fromMSecsSinceEpoch(lastTimeRounded));
    m_axisX->setTickCount(static_cast<int>(tickCountX));

    // 縦軸（Y軸）の目盛りを綺麗な整数刻み（1, 2, 5, 10, 25, 50...）にする丸め処理
    qint64 niceStep = 1;
    int chartHeight = ui->chartView->height();
    qint64 targetDivisionsY = qMax(2LL, static_cast<qint64>(chartHeight / 50));
    qint64 targetStep = (maxCount > 0) ? (maxCount + targetDivisionsY - 1) / targetDivisionsY : 1;

    if (targetStep <= 1) niceStep = 1;
    else if (targetStep <= 2) niceStep = 2;
    else if (targetStep <= 5) niceStep = 5;
    else if (targetStep <= 10) niceStep = 10;
    else if (targetStep <= 20) niceStep = 20;
    else if (targetStep <= 25) niceStep = 25;
    else if (targetStep <= 50) niceStep = 50;
    else if (targetStep <= 100) niceStep = 100;
    else if (targetStep <= 200) niceStep = 200;
    else if (targetStep <= 250) niceStep = 250;
    else if (targetStep <= 500) niceStep = 500;
    else {
        qint64 factor = 1000;
        while (true) {
            if (targetStep <= factor) { niceStep = factor; break; }
            if (targetStep <= factor * 2) { niceStep = factor * 2; break; }
            if (targetStep <= factor * 2.5) { niceStep = factor * 25 / 10; break; }
            if (targetStep <= factor * 5) { niceStep = factor * 5; break; }
            factor *= 10;
        }
    }

    qint64 maxRangeValue = ((maxCount + niceStep - 1) / niceStep) * niceStep;
    if (maxRangeValue == 0) maxRangeValue = niceStep;
    qint64 tickCount = (maxRangeValue / niceStep) + 1;

    m_axisY->setRange(0, maxRangeValue);
    m_axisY->setTickCount(static_cast<int>(tickCount));
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
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        
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

        connect(list, &QListWidget::itemSelectionChanged, this, &MainWindow::onListSelectionChanged);
    };

    createAccordion(m_btnToggleAll, m_listAll, "すべて");
    createAccordion(m_btnToggleBroadcaster, m_listBroadcaster, "ストリーマー");
    createAccordion(m_btnToggleModerator, m_listModerator, "モデレーター");
    createAccordion(m_btnToggleVip, m_listVip, "VIP");
    createAccordion(m_btnToggleArtist, m_listArtist, "アーティスト");
    createAccordion(m_btnToggleBot, m_listBot, "チャットボット");
    createAccordion(m_btnToggleRegular, m_listRegular, "一般");

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 更新ボタンのシグナル接続を非同期でコントローラ呼び出しへ変更
    connect(m_btnUpdateChatters, &QPushButton::clicked, this, [this]() {
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "triggerChattersFetch", Qt::QueuedConnection);
        }
    });

    m_originalWindowTitle = windowTitle();
    m_shoutoutCooldownTimer = new QTimer(this);
    m_shoutoutCooldownTimer->setInterval(1000);
    connect(m_shoutoutCooldownTimer, &QTimer::timeout, this, &MainWindow::onShoutoutCooldownTimeout);
}

void MainWindow::setUpdateButtonEnabled(bool enabled) {
    if (m_btnUpdateChatters) {
        m_btnUpdateChatters->setEnabled(enabled);
    }
}

void MainWindow::updateChattersList(const QList<TwitchEvents::ChatterInfo>& chatters) {
    if (!m_listAll) return;

    m_listAll->clear();
    m_listBroadcaster->clear();
    m_listModerator->clear();
    m_listVip->clear();
    m_listArtist->clear();
    m_listBot->clear();
    m_listRegular->clear();

    QStringList botUsers = m_configManager ? m_configManager->getBotUsers() : QStringList();
    QStringList lowerBots;
    for (const QString& b : botUsers) {
        lowerBots.append(b.toLower());
    }

    auto connectWidget = [this](ChatterRowWidget* w) {
        connect(w, &ChatterRowWidget::shoutoutClicked, this, &MainWindow::onChatterShoutoutClicked);
        connect(w, &ChatterRowWidget::vipToggled, this, &MainWindow::onChatterVipToggled);
        connect(w, &ChatterRowWidget::moderatorToggled, this, &MainWindow::onChatterModeratorToggled);
        connect(w, &ChatterRowWidget::timeoutClicked, this, &MainWindow::onChatterTimeoutClicked);
        connect(w, &ChatterRowWidget::banToggled, this, &MainWindow::onChatterBanToggled);
    };

    for (const auto& chatter : chatters) {
        QString name = chatter.userName;
        const QStringList& badges = chatter.userBadges;

        QList<QListWidget*> targetLists;
        if (lowerBots.contains(name.toLower())) {
            targetLists.append(m_listBot);
        }
        if (badges.contains("broadcaster")) {
            targetLists.append(m_listBroadcaster);
        }
        if (badges.contains("moderator")) {
            targetLists.append(m_listModerator);
        }
        if (badges.contains("vip")) {
            targetLists.append(m_listVip);
        }
        if (badges.contains("artist")) {
            targetLists.append(m_listArtist);
        }

        if (targetLists.isEmpty()) {
            targetLists.append(m_listRegular);
        }

        // すべてリストに追加
        QListWidgetItem* itemAll = new QListWidgetItem(m_listAll);
        itemAll->setSizeHint(QSize(0, 38));
        ChatterRowWidget* widgetAll = new ChatterRowWidget(chatter.userId, chatter.userName, chatter.userBadges, m_listAll);
        widgetAll->setShoutoutButtonEnabled(m_shoutoutCooldownRemaining <= 0);
        m_listAll->setItemWidget(itemAll, widgetAll);
        connectWidget(widgetAll);
        
        // 対象のカテゴリリストに追加
        for (QListWidget* targetList : targetLists) {
            QListWidgetItem* itemTarget = new QListWidgetItem(targetList);
            itemTarget->setSizeHint(QSize(0, 38));
            ChatterRowWidget* widgetTarget = new ChatterRowWidget(chatter.userId, chatter.userName, chatter.userBadges, targetList);
            widgetTarget->setShoutoutButtonEnabled(m_shoutoutCooldownRemaining <= 0);
            targetList->setItemWidget(itemTarget, widgetTarget);
            connectWidget(widgetTarget);
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
    updateBtnText(m_btnToggleArtist, m_listArtist, "アーティスト");
    updateBtnText(m_btnToggleBot, m_listBot, "チャットボット");
    updateBtnText(m_btnToggleRegular, m_listRegular, "一般");

    m_labelLastUpdate->setText("最終更新: " + QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::onChatterShoutoutClicked(const QString& userId, const QString& userName) {
    Q_UNUSED(userName);
    if (m_shoutoutCooldownRemaining > 0) return;

    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "onChatterShoutoutRequested", Qt::QueuedConnection, Q_ARG(QString, userId));
    }

    m_shoutoutCooldownRemaining = 120; // 2分間
    updateShoutoutButtonsEnabled(false);
    m_shoutoutCooldownTimer->start();
    onShoutoutCooldownTimeout(); // 初回描画
}

void MainWindow::onChatterVipToggled(const QString& userId, const QString& userName, bool checked) {
    Q_UNUSED(userName);
    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "onChatterVipToggled", Qt::QueuedConnection, Q_ARG(QString, userId), Q_ARG(bool, checked));
    }
}

void MainWindow::onChatterModeratorToggled(const QString& userId, const QString& userName, bool checked) {
    ChatterRowWidget* row = qobject_cast<ChatterRowWidget*>(sender());
    
    QString msg = checked 
        ? QString("本当に %1 をモデレーターに任命しますか？").arg(userName)
        : QString("本当に %1 からモデレーター権限を剥奪しますか？").arg(userName);
        
    auto res = QMessageBox::question(this, "モデレーター権限変更", msg, QMessageBox::Yes | QMessageBox::No);
    if (res == QMessageBox::Yes) {
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "onChatterModeratorToggled", Qt::QueuedConnection, Q_ARG(QString, userId), Q_ARG(bool, checked));
        }
    } else {
        if (row) {
            row->updateButtonsFromBadges(row->userBadges()); // 以前の状態に戻す
        }
    }
}

void MainWindow::onChatterTimeoutClicked(const QString& userId, const QString& userName) {
    bool ok;
    int minutes = QInputDialog::getInt(this, "タイムアウト時間設定", 
                                       QString("%1 をタイムアウトする時間（分）を入力してください:").arg(userName), 
                                       10, 1, 1440, 1, &ok);
    if (ok) {
        int seconds = minutes * 60;
        QString confirmMsg = QString("本当に %1 を %2 分間タイムアウトしますか？").arg(userName).arg(minutes);
        auto res = QMessageBox::question(this, "タイムアウト確認", confirmMsg, QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            if (m_controller) {
                QMetaObject::invokeMethod(m_controller, "onChatterTimeoutRequested", Qt::QueuedConnection, Q_ARG(QString, userId), Q_ARG(int, seconds));
            }
        }
    }
}

void MainWindow::onChatterBanToggled(const QString& userId, const QString& userName, bool checked) {
    ChatterRowWidget* row = qobject_cast<ChatterRowWidget*>(sender());
    
    if (checked) {
        QString msg = QString("本当に %1 を永久BANしますか？\n(過去のチャットもすべて削除されます)").arg(userName);
        auto res = QMessageBox::question(this, "永久BAN確認", msg, QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            if (m_controller) {
                QMetaObject::invokeMethod(m_controller, "onChatterBanToggled", Qt::QueuedConnection, Q_ARG(QString, userId), Q_ARG(bool, true));
            }
        } else {
            if (row) {
                row->updateButtonsFromBadges(row->userBadges()); // 戻す
            }
        }
    } else {
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "onChatterBanToggled", Qt::QueuedConnection, Q_ARG(QString, userId), Q_ARG(bool, false));
        }
    }
}

void MainWindow::onListSelectionChanged() {
    QListWidget* senderList = qobject_cast<QListWidget*>(sender());
    if (!senderList) return;

    QListWidgetItem* selectedItem = senderList->currentItem();
    ChatterRowWidget* selectedRow = selectedItem ? qobject_cast<ChatterRowWidget*>(senderList->itemWidget(selectedItem)) : nullptr;
    QString selectedUserId = selectedRow ? selectedRow->userId() : "";

    QListWidget* lists[] = { m_listAll, m_listBroadcaster, m_listModerator, m_listVip, m_listArtist, m_listBot, m_listRegular };
    for (QListWidget* list : lists) {
        if (!list) continue;
        
        // 他のリストで選択されている行も同じユーザーIDであれば選択状態にし、そうでなければ選択状態を外す
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            ChatterRowWidget* row = qobject_cast<ChatterRowWidget*>(list->itemWidget(item));
            if (row) {
                bool isSelectedUser = (!selectedUserId.isEmpty() && row->userId() == selectedUserId);
                row->setSelectedState(isSelectedUser);
                
                // 表示状態の選択ハイライトも連動
                if (list != senderList) {
                    list->blockSignals(true);
                    item->setSelected(isSelectedUser);
                    list->blockSignals(false);
                }
            }
        }
    }
}

void MainWindow::onShoutoutCooldownTimeout() {
    m_shoutoutCooldownRemaining--;
    if (m_shoutoutCooldownRemaining <= 0) {
        m_shoutoutCooldownTimer->stop();
        setWindowTitle(m_originalWindowTitle);
        updateShoutoutButtonsEnabled(true);
    } else {
        int min = m_shoutoutCooldownRemaining / 60;
        int sec = m_shoutoutCooldownRemaining % 60;
        setWindowTitle(m_originalWindowTitle + QString(" (シャウトアウト残り %1:%2)")
                       .arg(min)
                       .arg(sec, 2, 10, QChar('0')));
    }
}

void MainWindow::updateShoutoutButtonsEnabled(bool enabled) {
    QListWidget* lists[] = { m_listAll, m_listBroadcaster, m_listModerator, m_listVip, m_listArtist, m_listBot, m_listRegular };
    for (QListWidget* list : lists) {
        if (!list) continue;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            ChatterRowWidget* row = qobject_cast<ChatterRowWidget*>(list->itemWidget(item));
            if (row) {
                row->setShoutoutButtonEnabled(enabled);
            }
        }
    }
}

void MainWindow::addChatterFromComment(const QString& userId, const QString& userName, const QStringList& badges) {
    if (!m_listAll) return;

    // すでに「すべて」リストに存在するかチェック
    bool exists = false;
    for (int i = 0; i < m_listAll->count(); ++i) {
        QListWidgetItem* item = m_listAll->item(i);
        ChatterRowWidget* row = qobject_cast<ChatterRowWidget*>(m_listAll->itemWidget(item));
        if (row && row->userId() == userId) {
            exists = true;
            break;
        }
    }

    if (exists) return; // 既に存在していれば何もしない

    // 新しく追加するための振り分け先リストを決定
    QStringList botUsers = m_configManager ? m_configManager->getBotUsers() : QStringList();
    QStringList lowerBots;
    for (const QString& b : botUsers) {
        lowerBots.append(b.toLower());
    }

    QList<QListWidget*> targetLists;
    if (lowerBots.contains(userName.toLower())) {
        targetLists.append(m_listBot);
    }
    if (badges.contains("broadcaster")) {
        targetLists.append(m_listBroadcaster);
    }
    if (badges.contains("moderator")) {
        targetLists.append(m_listModerator);
    }
    if (badges.contains("vip")) {
        targetLists.append(m_listVip);
    }
    if (badges.contains("artist")) {
        targetLists.append(m_listArtist);
    }

    if (targetLists.isEmpty()) {
        targetLists.append(m_listRegular);
    }

    auto connectWidget = [this](ChatterRowWidget* w) {
        connect(w, &ChatterRowWidget::shoutoutClicked, this, &MainWindow::onChatterShoutoutClicked);
        connect(w, &ChatterRowWidget::vipToggled, this, &MainWindow::onChatterVipToggled);
        connect(w, &ChatterRowWidget::moderatorToggled, this, &MainWindow::onChatterModeratorToggled);
        connect(w, &ChatterRowWidget::timeoutClicked, this, &MainWindow::onChatterTimeoutClicked);
        connect(w, &ChatterRowWidget::banToggled, this, &MainWindow::onChatterBanToggled);
    };

    // すべてリストに追加
    QListWidgetItem* itemAll = new QListWidgetItem(m_listAll);
    itemAll->setSizeHint(QSize(0, 38));
    ChatterRowWidget* widgetAll = new ChatterRowWidget(userId, userName, badges, m_listAll);
    widgetAll->setShoutoutButtonEnabled(m_shoutoutCooldownRemaining <= 0);
    m_listAll->setItemWidget(itemAll, widgetAll);
    connectWidget(widgetAll);

    // 対象のカテゴリリストに追加
    for (QListWidget* targetList : targetLists) {
        QListWidgetItem* itemTarget = new QListWidgetItem(targetList);
        itemTarget->setSizeHint(QSize(0, 38));
        ChatterRowWidget* widgetTarget = new ChatterRowWidget(userId, userName, badges, targetList);
        widgetTarget->setShoutoutButtonEnabled(m_shoutoutCooldownRemaining <= 0);
        targetList->setItemWidget(itemTarget, widgetTarget);
        connectWidget(widgetTarget);
    }

    // アコーディオンの件数表示を更新
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
    updateBtnText(m_btnToggleArtist, m_listArtist, "アーティスト");
    updateBtnText(m_btnToggleBot, m_listBot, "チャットボット");
    updateBtnText(m_btnToggleRegular, m_listRegular, "一般");
}

void MainWindow::showCommentContextMenu(const QPoint& pos) {
    QModelIndex idx = ui->tableViewComments->indexAt(pos);
    if (!idx.isValid()) return;

    int row = idx.row();
    QModelIndex msgIdx = m_chatModel->index(row, 1);
    QString messageId = m_chatModel->data(msgIdx, Qt::UserRole + 1).toString();
    if (messageId.isEmpty()) return;

    QMenu menu(this);
    
    QMenu* pinSubMenu = menu.addMenu("📌 チャット上部にピン留めする");
    QAction* act30s = pinSubMenu->addAction("30秒");
    QAction* act1m = pinSubMenu->addAction("1分");
    QAction* act5m = pinSubMenu->addAction("5分");
    QAction* act15m = pinSubMenu->addAction("15分");
    QAction* act30m = pinSubMenu->addAction("30分");
    QAction* actEnd = pinSubMenu->addAction("配信終了まで");

    QAction* actUnpin = nullptr;
    if (!m_currentlyPinnedMessageId.isEmpty()) {
        actUnpin = menu.addAction("❌ ピン留めを解除");
    }

    QAction* selected = menu.exec(ui->tableViewComments->viewport()->mapToGlobal(pos));
    if (!selected) return;

    if (selected == actUnpin) {
        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "onUnpinCommentRequested", Qt::QueuedConnection, Q_ARG(QString, m_currentlyPinnedMessageId));
        }
        m_currentlyPinnedMessageId = "";
    } else {
        int seconds = 0;
        if (selected == act30s) seconds = 30;
        else if (selected == act1m) seconds = 60;
        else if (selected == act5m) seconds = 300;
        else if (selected == act15m) seconds = 900;
        else if (selected == act30m) seconds = 1800;
        else if (selected == actEnd) seconds = 0;

        if (m_controller) {
            QMetaObject::invokeMethod(m_controller, "onPinCommentRequested", Qt::QueuedConnection, Q_ARG(QString, messageId), Q_ARG(int, seconds));
        }
        m_currentlyPinnedMessageId = messageId;
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    m_chartNeedsUpdate = true;
    updateChartDisplay();
}

void MainWindow::on_btnSendAnnouncement_clicked() {
    QString text = ui->lineAnnouncementText->text().trimmed();
    if (text.isEmpty()) return;

    // Map UI color names to Twitch Helix Announcement API colors
    // Combo indices: 0:通常 (primary), 1:青 (blue), 2:緑 (green), 3:オレンジ (orange), 4:紫 (purple)
    QString color = "primary";
    int index = ui->comboAnnouncementColor->currentIndex();
    if (index == 1) color = "blue";
    else if (index == 2) color = "green";
    else if (index == 3) color = "orange";
    else if (index == 4) color = "purple";

    if (m_controller) {
        QMetaObject::invokeMethod(m_controller, "onSendAnnouncementRequested",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, text),
                                  Q_ARG(QString, color));
        ui->lineAnnouncementText->clear();
    }
}

void MainWindow::on_lineAnnouncementText_returnPressed() {
    on_btnSendAnnouncement_clicked();
}

void MainWindow::markTtsSettingsUnsaved(bool unsaved) {
    if (unsaved) {
        ui->btnSaveTts->setText("設定を保存 (要保存*)");
        ui->btnSaveTts->setStyleSheet("QPushButton { font-weight: bold; color: #ff5500; border: 1px solid #ff5500; }");
    } else {
        ui->btnSaveTts->setText("設定を保存");
        ui->btnSaveTts->setStyleSheet("");
    }
}

void MainWindow::onStreamSessionChanged(int index) {
    if (!m_dbManager || index < 0 || !m_comboStreamSession) return;

    qint64 sessionId = m_comboStreamSession->itemData(index).toLongLong();
    QDateTime start, end;

    if (sessionId != -1) {
        for (const auto& session : m_sessions) {
            if (session.id == sessionId) {
                start = session.startTime;
                end = session.endTime;
                break;
            }
        }
    }

    m_commentHistory.clear();
    QList<CommentLog> logs = m_dbManager->getComments(start, end);
    
    int total = 0;
    QMap<QString, int> userCounts;
    
    for (const CommentLog& log : logs) {
        LogEntry entry;
        entry.time = log.timestamp.toLocalTime();
        entry.user = log.userName;
        m_commentHistory.append(entry);
        
        total++;
        userCounts[log.userName]++;
    }
    
    updateStatistics(total, userCounts);
    m_chartNeedsUpdate = true;
    updateChartDisplay();
}

void MainWindow::refreshSessionsComboBox() {
    if (!m_dbManager || !m_comboStreamSession) return;

    m_comboStreamSession->blockSignals(true);
    m_comboStreamSession->clear();

    m_sessions = m_dbManager->getStreamSessions();

    m_comboStreamSession->addItem("全期間", -1);

    for (const auto& session : m_sessions) {
        QString timeStr = session.startTime.toString("yyyy/MM/dd HH:mm");
        if (session.endTime.isValid()) {
            timeStr += " ~ " + session.endTime.toString("HH:mm");
        } else {
            timeStr += " ~ (配信中)";
        }
        m_comboStreamSession->addItem(timeStr, session.id);
    }

    m_comboStreamSession->blockSignals(false);
    
    if (m_activeSessionId != -1) {
        int targetIndex = 0;
        for (int i = 0; i < m_comboStreamSession->count(); ++i) {
            if (m_comboStreamSession->itemData(i).toLongLong() == m_activeSessionId) {
                targetIndex = i;
                break;
            }
        }
        m_comboStreamSession->setCurrentIndex(targetIndex);
    } else {
        m_comboStreamSession->setCurrentIndex(0);
    }
}

void MainWindow::handleStreamStatusChanged(bool online, qint64 activeSessionId) {
    m_activeSessionId = activeSessionId;
    if (online) {
        showStatusMessage("配信の開始イベントを受信しました。新セッションを開始します。");
    } else {
        showStatusMessage("配信の終了イベントを受信しました。セッションを終了します。");
    }
    refreshSessionsComboBox();
}

void MainWindow::on_comboObsOverlay_currentTextChanged(const QString& text) {
    bool isPhysics = (text == "physics/index.html");
    ui->groupBoxObsPhysics->setVisible(isPhysics);
}

void MainWindow::on_spinObsAvatarMinSize_valueChanged(int value) {
    if (value > ui->spinObsAvatarMaxSize->value()) {
        ui->spinObsAvatarMaxSize->setValue(value);
    }
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsAvatarMaxSize_valueChanged(int value) {
    if (value < ui->spinObsAvatarMinSize->value()) {
        ui->spinObsAvatarMinSize->setValue(value);
    }
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsBounceFactor_valueChanged(int value) {
    Q_UNUSED(value);
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsBrowserWidth_valueChanged(int value) {
    Q_UNUSED(value);
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsBrowserHeight_valueChanged(int value) {
    Q_UNUSED(value);
    updateObsPhysicsPreview();
}

void MainWindow::on_editObsEffectSymbols_textChanged(const QString& text) {
    Q_UNUSED(text);
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsEffectSize_valueChanged(int value) {
    Q_UNUSED(value);
    updateObsPhysicsPreview();
}

void MainWindow::on_spinObsEffectCount_valueChanged(int value) {
    Q_UNUSED(value);
    updateObsPhysicsPreview();
}

void MainWindow::updateObsPhysicsPreview() {
    int minSize = ui->spinObsAvatarMinSize->value();
    int maxSize = ui->spinObsAvatarMaxSize->value();
    int bounce = ui->spinObsBounceFactor->value();
    ui->lblObsPhysicsPreview->setText(QString("サイズプレビュー: [ %1px 〜 %2px ]").arg(minSize).arg(maxSize));

    // WebSocket経由で即時ブロードキャスト
    if (m_controller && m_configManager && ui->comboObsOverlay->currentText() == "physics/index.html") {
        QVariantMap payload;
        payload["minSize"] = minSize;
        payload["maxSize"] = maxSize;
        payload["bounceFactor"] = bounce;
        payload["obsBrowserWidth"] = ui->spinObsBrowserWidth->value();
        payload["obsBrowserHeight"] = ui->spinObsBrowserHeight->value();
        payload["effectSymbols"] = ui->editObsEffectSymbols->text();
        payload["effectSize"] = ui->spinObsEffectSize->value();
        payload["effectCount"] = ui->spinObsEffectCount->value();
        QMetaObject::invokeMethod(m_controller, "onBroadcastObsActionRequested", Qt::QueuedConnection,
                                  Q_ARG(QString, "settings_changed"),
                                  Q_ARG(QVariantMap, payload));
    }
}
