#include <QApplication>
#include <QStatusBar>
#include <QByteArray>
#include "MainWindow.h"
#include "AppController.h"
#include <TrustChainCore.hpp>
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
        // ① タイトルバーの自動書き換え
        w.setWindowTitle(w.windowTitle() + " (Custom Build)");

        // ② 難読化されたコピーライト文字列を TransCipher で復号
        QByteArray encryptedData = QByteArray::fromHex(WATERMARK_ENCRYPTED_TEXT);
        CipherResult result = CipherEngine::decrypt(encryptedData, WATERMARK_DECRYPT_KEY);
        
        QString copyrightText;
        if (result.isSuccess()) {
            copyrightText = QString("© %1 (Original Creator)").arg(QString::fromUtf8(result.data()));
        } else {
            copyrightText = "© Original Creator (Protected Build)";
        }

        // ③ ステータスバーの強制保護とコピーライト表示
        QStatusBar* statusBar = w.statusBar();
        if (statusBar) {
            statusBar->showMessage(copyrightText);
            statusBar->setStyleSheet(
                "color: #888888; "
                "background-color: #121214; "
                "font-weight: bold; "
                "border-top: 1px solid #2d2d30;"
            );
            statusBar->setVisible(true);
        }
    }

    // AppController の初期化と稼働開始
    AppController appController;
    appController.setMainWindow(&w);
    appController.initialize();

    w.show();
    return a.exec();
}
