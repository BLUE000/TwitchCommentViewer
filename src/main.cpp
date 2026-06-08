#include <QApplication>
#include <QStatusBar>
#include <QByteArray>
#include "MainWindow.h"
#include "AppController.h"
#include <TrustChainCore.hpp>
#include <TrustChainQt.hpp>
#include <cipher_engine.h>

#ifndef WATERMARK_ENCRYPTED_TEXT
#define WATERMARK_ENCRYPTED_TEXT "dummy"
#endif

#ifndef WATERMARK_DECRYPT_KEY
#define WATERMARK_DECRYPT_KEY "dummy_key"
#endif

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. TrustChain ネット照会・出自証明ガードマンを生成
    TrustChain::Core guard;
    
    // 2. サーバーに問い合わせ、トークン検証およびヌル文字汚染スキャンを実行
    TrustChain::AuthStatus status = guard.verifyToken();

    // 3. メイン画面の生成
    MainWindow w;

    // 4. 検証結果に基づくウォーターマーク強制付与（改ざん検知時）
    if (status == TrustChain::AuthStatus::Terminated) {
        TrustChain::Core::terminateApplication("このビルドはサーバー側で無効化されているため、起動できません。");
        return 0;
    }
    else if (status == TrustChain::AuthStatus::Watermarked) {
        // TransCipher によるコピーライトの復号
        QString customCopyright;
        QByteArray encryptedData = QByteArray::fromHex(WATERMARK_ENCRYPTED_TEXT);
        CipherResult result = CipherEngine::decrypt(encryptedData, WATERMARK_DECRYPT_KEY);
        if (result.isSuccess()) {
            customCopyright = QString::fromUtf8(result.data());
        }
        
        // QtHelper を用いてウォーターマーク（タイトルとステータスバーへの表記）を適用
        TrustChain::QtHelper::applyWatermark(&w, status, customCopyright);
    }

    // AppController の初期化と稼働開始
    AppController appController;
    appController.setMainWindow(&w);
    appController.initialize();

    w.show();
    return a.exec();
}
