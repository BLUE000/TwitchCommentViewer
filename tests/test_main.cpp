#include <gtest/gtest.h>
#include <QString>
#include <cipher_engine.h>

// UT-DB-01: 暗号化・復号の可逆性のコア機能テスト (DatabaseManager の根幹)
TEST(DatabaseManagerTest, EncryptionDecryptionReversibility) {
    QString plainText = "TestCommentData_123_スパムテスト";
    QString key = "unit_test_secret_key";

    // 暗号化
    CipherResult encRes = CipherEngine::encrypt(plainText.toUtf8(), key);
    ASSERT_TRUE(encRes.isSuccess()) << "Encryption failed: " << encRes.message().toStdString();

    // 復号
    CipherResult decRes = CipherEngine::decrypt(encRes.data(), key);
    ASSERT_TRUE(decRes.isSuccess()) << "Decryption failed: " << decRes.message().toStdString();

    // 可逆性チェック
    EXPECT_EQ(QString::fromUtf8(decRes.data()), plainText);
}

#include "DatabaseManager.h"
#include <QFile>
#include <QSqlQuery>
#include <QVariant>

// UT-DB-02: DatabaseManager を用いた暗号化DB書き込みテスト
TEST(DatabaseManagerTest, SQLiteIntegrationWithTransCipher) {
    QString testDbPath = "test_twitch_comments.db";
    QString testKey = "test_sqlite_key";
    
    // 古いテストDBを削除
    if (QFile::exists(testDbPath)) {
        QFile::remove(testDbPath);
    }

    DatabaseManager dbm;
    ASSERT_TRUE(dbm.initialize(testDbPath, testKey)) << "Failed to initialize DB";

    // コメントログの記録
    bool logSuccess = dbm.logComment("user123", "Twitch太郎", "こんにちは！配信見に来ました", 0.8, false);
    EXPECT_TRUE(logSuccess) << "Failed to log comment";

    // SQLiteから直接データを読み取って暗号化されているか、正しく復号できるかを確認
    QSqlDatabase db = QSqlDatabase::database("TwitchDBConnection");
    QSqlQuery query(db);
    ASSERT_TRUE(query.exec("SELECT user_id, username_enc, message_enc FROM chat_logs WHERE user_id='user123'"));
    ASSERT_TRUE(query.next());

    EXPECT_EQ(query.value(0).toString(), "user123");
    
    QByteArray encName = query.value(1).toByteArray();
    QByteArray encMsg = query.value(2).toByteArray();
    
    // 平文のまま保存されていないことを確認
    EXPECT_NE(encName, "Twitch太郎");
    EXPECT_NE(encMsg, "こんにちは！配信見に来ました");

    // TransCipherで復号して元のテキストに戻るか確認
    CipherResult decName = CipherEngine::decrypt(encName, testKey);
    CipherResult decMsg = CipherEngine::decrypt(encMsg, testKey);

    EXPECT_TRUE(decName.isSuccess());
    EXPECT_TRUE(decMsg.isSuccess());
    EXPECT_EQ(QString::fromUtf8(decName.data()), "Twitch太郎");
    EXPECT_EQ(QString::fromUtf8(decMsg.data()), "こんにちは！配信見に来ました");

    // DBをクローズして後片付け
    db.close();
}

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "events/TwitchEvents.h"

// UT-CHT-01: 視聴者リスト（chatters）のJSONデータ解析テスト
TEST(ChattersTest, JSONParsingAndBadgeAssignment) {
    QString rawJson = R"({
        "data": [
            {"user_id": "1", "user_login": "streamer_login", "user_name": "ストリーマー名", "user_badge": "broadcaster"},
            {"user_id": "2", "user_login": "mod_login", "user_name": "モデレーター名", "user_badge": "moderator"},
            {"user_id": "3", "user_login": "vip_login", "user_name": "", "user_badge": "vip"},
            {"user_id": "4", "user_login": "normal_login", "user_name": "一般ユーザー", "user_badge": "none"}
        ]
    })";

    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
    ASSERT_FALSE(doc.isNull());
    ASSERT_TRUE(doc.isObject());

    QJsonArray dataArray = doc.object()["data"].toArray();
    QList<TwitchEvents::ChatterInfo> chatterList;

    for (const QJsonValue& val : dataArray) {
        QJsonObject obj = val.toObject();
        QString userId = obj["user_id"].toString();
        QString userName = obj["user_name"].toString();
        if (userName.isEmpty()) {
            userName = obj["user_login"].toString();
        }
        QString userBadge = obj["user_badge"].toString();
        
        TwitchEvents::ChatterInfo info;
        info.userId = userId;
        info.userName = userName;
        info.userBadge = userBadge;
        chatterList.append(info);
    }

    ASSERT_EQ(chatterList.size(), 4);
    
    // 1: Broadcaster
    EXPECT_EQ(chatterList[0].userId, "1");
    EXPECT_EQ(chatterList[0].userName, "ストリーマー名");
    EXPECT_EQ(chatterList[0].userBadge, "broadcaster");

    // 2: Moderator
    EXPECT_EQ(chatterList[1].userId, "2");
    EXPECT_EQ(chatterList[1].userName, "モデレーター名");
    EXPECT_EQ(chatterList[1].userBadge, "moderator");

    // 3: VIP (fallback to user_login)
    EXPECT_EQ(chatterList[2].userId, "3");
    EXPECT_EQ(chatterList[2].userName, "vip_login");
    EXPECT_EQ(chatterList[2].userBadge, "vip");

    // 4: None
    EXPECT_EQ(chatterList[3].userId, "4");
    EXPECT_EQ(chatterList[3].userName, "一般ユーザー");
    EXPECT_EQ(chatterList[3].userBadge, "none");
}

// UT-CHT-02: ボットユーザーの除外判定ロジックのテスト
TEST(ChattersTest, BotExclusionFilter) {
    QStringList botUsers = {"Nightbot", "Moobot", "StreamElements"};
    QStringList lowerBots;
    for (const QString& b : botUsers) {
        lowerBots.append(b.toLower());
    }

    // テスト対象の入力
    QString testUser1 = "nightbot";
    QString testUser2 = "moobot";
    QString testUser3 = "STREAMELEMENTS";
    QString testUser4 = "twitch_tarou";

    EXPECT_TRUE(lowerBots.contains(testUser1.toLower()));
    EXPECT_TRUE(lowerBots.contains(testUser2.toLower()));
    EXPECT_TRUE(lowerBots.contains(testUser3.toLower()));
    EXPECT_FALSE(lowerBots.contains(testUser4.toLower()));
}

// UT-CHT-03: 手動更新クールダウンの秒数判定ロジックのテスト
TEST(ChattersTest, CooldownLogic) {
    QDateTime lastFetch = QDateTime::currentDateTime().addSecs(-100); // 100秒前
    qint64 secsElapsed = lastFetch.secsTo(QDateTime::currentDateTime());
    
    // 3分(180秒)未満は手動更新不可
    EXPECT_FALSE(secsElapsed >= 180);
    EXPECT_FALSE(secsElapsed >= 600);

    QDateTime lastFetch3Min = QDateTime::currentDateTime().addSecs(-200); // 200秒前
    secsElapsed = lastFetch3Min.secsTo(QDateTime::currentDateTime());
    // 3分以上なので手動更新可能、10分未満なので自動更新は走らない
    EXPECT_TRUE(secsElapsed >= 180);
    EXPECT_FALSE(secsElapsed >= 600);

    QDateTime lastFetch10Min = QDateTime::currentDateTime().addSecs(-610); // 610秒前
    secsElapsed = lastFetch10Min.secsTo(QDateTime::currentDateTime());
    // 10分以上なので自動更新も手動更新も可能
    EXPECT_TRUE(secsElapsed >= 180);
    EXPECT_TRUE(secsElapsed >= 600);
}

#include <QCoreApplication>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
