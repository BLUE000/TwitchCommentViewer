#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "modules/ConfigManager.h"
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_chatModel(new QStandardItemModel(0, 2, this))
{
    ui->setupUi(this);

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
    
    m_configManager->saveConfig();
    QMessageBox::information(this, "設定保存", "棒読みちゃんの設定を保存しました。");
}
