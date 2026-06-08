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

#include <QCoreApplication>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
