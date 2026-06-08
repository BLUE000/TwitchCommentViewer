#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "modules/ConfigManager.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QClipboard>
#include <QUrl>
#include <QDir>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chatModel(new QStandardItemModel(0, 2, this))
{
    ui->setupUi(this);

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

    // ビューワタブのテーブル設定
    m_chatModel->setHeaderData(0, Qt::Horizontal, "Username");
    m_chatModel->setHeaderData(1, Qt::Horizontal, "Message");
    ui->tableViewComments->setModel(m_chatModel);
    ui->tableViewComments->setColumnWidth(0, 150);
    ui->tableViewComments->horizontalHeader()->setStretchLastSection(true);

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
    QString aboutText = 
        "<h2>TwitchCommentManager</h2>"
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
