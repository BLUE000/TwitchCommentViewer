#include "ViewerMainWindow.h"
#include "ui_ViewerMainWindow.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDateTime>
#include <cipher_engine.h>
#include <QDebug>

ViewerMainWindow::ViewerMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ViewerMainWindow)
    , m_model(new QStandardItemModel(this))
{
    ui->setupUi(this);

    // ヘッダーの設定
    m_model->setHorizontalHeaderLabels({
        "日時", "ユーザーID", "ユーザー名", "コメント内容", "スコア", "スパム判定"
    });
    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);

    // 起動時にロード
    loadDatabase();
}

ViewerMainWindow::~ViewerMainWindow() {
    delete ui;
}

void ViewerMainWindow::on_btnReload_clicked() {
    loadDatabase();
}

void ViewerMainWindow::loadDatabase() {
    m_model->removeRows(0, m_model->rowCount());

    QString dbPath = QCoreApplication::applicationDirPath() + "/twitch_comments.db";
    const QString DB_CONNECTION_NAME = "DBViewerConnection";
    const QString RUNTIME_KEY = "application_runtime_key"; // 固定キー

    // 既に接続がある場合はクローズして削除
    if (QSqlDatabase::contains(DB_CONNECTION_NAME)) {
        QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", DB_CONNECTION_NAME);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        QMessageBox::warning(this, "エラー", "データベースを開けませんでした。\n" + dbPath);
        return;
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT timestamp, user_id, username_enc, message_enc, emotion_score, is_spam FROM chat_logs ORDER BY timestamp DESC")) {
        QMessageBox::warning(this, "エラー", "テーブルからの読み込みに失敗しました。\n" + query.lastError().text());
        db.close();
        return;
    }

    while (query.next()) {
        qint64 ts = query.value(0).toLongLong();
        QString userId = query.value(1).toString();
        QByteArray usernameEnc = query.value(2).toByteArray();
        QByteArray messageEnc = query.value(3).toByteArray();
        double emotionScore = query.value(4).toDouble();
        bool isSpam = query.value(5).toBool();

        // 復号処理
        CipherResult resUser = CipherEngine::decrypt(usernameEnc, RUNTIME_KEY);
        CipherResult resMsg = CipherEngine::decrypt(messageEnc, RUNTIME_KEY);

        QString username = resUser.isSuccess() ? QString::fromUtf8(resUser.data()) : "(復号エラー)";
        QString message = resMsg.isSuccess() ? QString::fromUtf8(resMsg.data()) : "(復号エラー)";

        QString timeStr = QDateTime::fromMSecsSinceEpoch(ts).toString("yyyy/MM/dd HH:mm:ss");

        QList<QStandardItem*> row;
        row << new QStandardItem(timeStr);
        row << new QStandardItem(userId);
        row << new QStandardItem(username);
        row << new QStandardItem(message);
        row << new QStandardItem(QString::number(emotionScore, 'f', 2));
        row << new QStandardItem(isSpam ? "SPAM" : "");

        m_model->appendRow(row);
    }

    db.close();
    ui->tableView->resizeColumnsToContents();
}
