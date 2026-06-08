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

ViewerMainWindow::ViewerMainWindow(const QString& dbPath, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ViewerMainWindow)
    , m_model(new QStandardItemModel(this))
    , m_dbPath(dbPath)
{
    ui->setupUi(this);

    // ヘッダーの設定
    m_model->setHorizontalHeaderLabels({
        "日時", "ユーザーID", "ユーザー名", "コメント内容", "スコア", "スパム判定"
    });
    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

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

    QString dbPath = m_dbPath;
    if (dbPath.isEmpty()) {
        dbPath = QCoreApplication::applicationDirPath() + "/twitch_comments.db";
    }
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
    if (!query.exec("SELECT id, timestamp, user_id, username_enc, message_enc, sentiment_score, is_spam FROM chat_logs ORDER BY timestamp DESC")) {
        QMessageBox::warning(this, "エラー", "テーブルからの読み込みに失敗しました。\n" + query.lastError().text());
        db.close();
        return;
    }

    while (query.next()) {
        qint64 id = query.value(0).toLongLong();
        qint64 ts = query.value(1).toLongLong();
        QString userId = query.value(2).toString();
        QByteArray usernameEnc = query.value(3).toByteArray();
        QByteArray messageEnc = query.value(4).toByteArray();
        double emotionScore = query.value(5).toDouble();
        bool isSpam = query.value(6).toBool();

        // 復号処理
        CipherResult resUser = CipherEngine::decrypt(usernameEnc, RUNTIME_KEY);
        CipherResult resMsg = CipherEngine::decrypt(messageEnc, RUNTIME_KEY);

        QString username = resUser.isSuccess() ? QString::fromUtf8(resUser.data()) : "(復号エラー)";
        QString message = resMsg.isSuccess() ? QString::fromUtf8(resMsg.data()) : "(復号エラー)";

        QString timeStr = QDateTime::fromMSecsSinceEpoch(ts).toString("yyyy/MM/dd HH:mm:ss");

        QList<QStandardItem*> row;
        QStandardItem* timeItem = new QStandardItem(timeStr);
        timeItem->setData(id, Qt::UserRole); // store ID in UserRole
        row << timeItem;
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

void ViewerMainWindow::on_btnDeleteSelected_clicked() {
    QModelIndexList selected = ui->tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "通知", "削除する行を選択してください。");
        return;
    }

    if (QMessageBox::question(this, "確認", QString("%1件のコメントを削除しますか？").arg(selected.size())) != QMessageBox::Yes) {
        return;
    }

    QString dbPath = m_dbPath.isEmpty() ? QCoreApplication::applicationDirPath() + "/twitch_comments.db" : m_dbPath;
    QSqlDatabase db = QSqlDatabase::database("DBViewerConnection");
    if (!db.isOpen()) {
        db = QSqlDatabase::addDatabase("QSQLITE", "DBViewerConnection");
        db.setDatabaseName(dbPath);
        if (!db.open()) return;
    }

    QSqlQuery query(db);
    db.transaction();
    bool success = true;
    for (const QModelIndex& index : selected) {
        qint64 id = m_model->item(index.row(), 0)->data(Qt::UserRole).toLongLong();
        query.prepare("DELETE FROM chat_logs WHERE id = :id");
        query.bindValue(":id", id);
        if (!query.exec()) {
            success = false;
            break;
        }
    }

    if (success) {
        db.commit();
        QMessageBox::information(this, "成功", "選択したコメントを削除しました。");
        loadDatabase();
    } else {
        db.rollback();
        QMessageBox::warning(this, "エラー", "削除中にエラーが発生しました。\n" + query.lastError().text());
    }
}

void ViewerMainWindow::on_btnDeleteAll_clicked() {
    if (QMessageBox::question(this, "確認", "すべてのコメント履歴を削除しますか？\nこの操作は元に戻せません。", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QString dbPath = m_dbPath.isEmpty() ? QCoreApplication::applicationDirPath() + "/twitch_comments.db" : m_dbPath;
    QSqlDatabase db = QSqlDatabase::database("DBViewerConnection");
    if (!db.isOpen()) {
        db = QSqlDatabase::addDatabase("QSQLITE", "DBViewerConnection");
        db.setDatabaseName(dbPath);
        if (!db.open()) return;
    }

    QSqlQuery query(db);
    if (query.exec("DELETE FROM chat_logs")) {
        QMessageBox::information(this, "成功", "すべてのコメント履歴を削除しました。");
        loadDatabase();
    } else {
        QMessageBox::warning(this, "エラー", "削除中にエラーが発生しました。\n" + query.lastError().text());
    }
}
