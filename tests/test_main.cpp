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
