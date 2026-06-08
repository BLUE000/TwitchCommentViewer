#include "MainWindow.h"
#include "ui_MainWindow.h"

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
