# Twitchコメント解析ツール 詳細設計書

ウォーターフォール Vモデルにおける「詳細設計（モジュール内部設計・DBスキーマ・クラス設計）」を定義します。この設計をインプットとして、Unit Test（単体テスト）および実装フェーズ（コーディング）に移行します。

## 1. セキュリティ設計・ビルド構成 (TrustChain導入)

本アプリケーションの「出自証明」および「改ざん防止・再配布対策」として、**TrustChain** を導入します。

* **CMake構成**: 
  * プロジェクトのサブディレクトリとして `TrustChain` を追加（`add_subdirectory`）し、ビルド時に自動で出自確認およびトークン埋め込みを実施する。
  * メインアプリのほか、後述する**付属ツール（DB Viewer）**も別EXEとして同一プロジェクト（CMake）内でビルド可能にする。
  * `trustchain_credentials.cmake` を利用し、GitHubリポジトリおよびTransCipherサーバーのURL、秘密鍵を安全に管理（Git管理外とする）。
* **第3層プロテクト（バイナリ透かし）**:
  * アプリ（`.exe`）完成後、ビルドプロセスに同梱される `BinMarkManager` を利用して物理的な権利情報（ウォーターマーク）をバイナリに埋め込むフローを採用する。

## 2. 画面設計 (UI Layout)
`MainWindow.ui` の構成要素：
* **基盤**: `QMainWindow`
* **セキュリティ対応**: 
  * アプリ起動時に `TrustChain::QtHelper::applyWatermark(&w, status)` を適用。改ざんやオフライン状態が検知された場合は、タイトルバーに `(Custom Build)` を付与し、ステータスバーに消去不可の権利表記を強制表示する。
* **メイン領域**: `QTabWidget`（以下の4タブ構成）
  1. **コメントタブ**: `QTableView` または `QListView`（コメントをリアルタイム表示、旧ビューワタブ）
  2. **視聴者タブ**: 
     * 手動「更新」ボタン、および最終更新時間ラベル。
     * `QScrollArea` と 6つのアコーディオン（すべて/ストリーマー/モデレーター/VIP/チャットボット/一般）。
     * 各アコーディオンは `QPushButton`（トグル）と `QListWidget` で構成。
     * ストリーマーを除く視聴者行にカスタム行ウィジェット（`ChatterRowWidget`）を配置し、マウスホバー時または行選択時に右端にアクションボタンを表示。
       - `[シャウトアウト] [VIP] [モデレータ]  (余白)  [タイムアウト] [BAN]` の配置と余白制御。
       - VIP・モデレータ・BANはトグル動作とし、状態を表現。
       - タイムアウト時は時間指定ダイアログを起動。
       - モデレータ追加/削除、タイムアウト決定後、BAN時には実行確認メッセージ（`QMessageBox`）を表示。
       - シャウトアウト時は、2分間のクールタイム表示（ウィンドウタイトルバーでのリアルタイムカウントダウン）を制御。
  3. **解析タブ**: 
     * `QTextBrowser`（トレンド・スパム等のログ表示）
     * `QtCharts`を用いた折れ線グラフ（単位時間あたりの総コメント数、または特定ユーザーごとのコメント推移を表示。粒度変更やTopN絞り込みに対応）
  4. **設定タブ**: Twitch接続設定、BOT設定、**権利・ライセンス表記**（ここにTrustChain・Qt等のライセンスを記載）
* **下部領域**: `QStatusBar`（接続状態やシステムメッセージの表示）

## 3. データ保護とデータベーススキーマ設計

### 3.1 中間ファイル・設定ファイルの難読化
プライバシー保護およびセキュリティの観点から、DBのみならず**システムが生成する中間ファイル（ログの一時キャッシュ、設定ファイル、集計用の一時エクスポート等）**についても、`TransCipher-Dist` を利用して難読化（暗号化）して保存します。これにより、平文ファイルからの意図しない情報漏洩や、外部ツールからの解析を防ぎます。

### 3.2 データベーススキーマ設計 (SQLite)
個人特定が可能なカラムも同様に `TransCipher-Dist` により暗号化して保存します。

#### テーブル: `chat_logs`
* `id` (INTEGER PRIMARY KEY AUTOINCREMENT)
* `timestamp` (DATETIME) - 受信日時
* `user_id` (TEXT) - TwitchのユーザーID（一意）
* `username_enc` (BLOB) - 発言者名（暗号化済）
* `message_enc` (BLOB) - 発言内容（暗号化済）
* `sentiment_score` (REAL) - 感情スコア（-1.0 ~ 1.0）
* `is_spam` (INTEGER) - スパム判定フラグ（0=正常, 1=スパム）

#### テーブル: `user_stats`
* `user_id` (TEXT PRIMARY KEY) - TwitchのユーザーID
* `username_enc` (BLOB) - 発言者名（暗号化済）
* `total_comments` (INTEGER) - 累計コメント数
* `last_seen` (DATETIME) - 最終発言日時

## 4. クラス設計・インターフェース設計

外部システムとの境界となるモジュールは、テスト容易性（Mocking）のため純粋仮想クラス（インターフェース）を定義します。

### 4.1 アプリケーション初期化層 (main.cpp)
* `main()` 関数内において、UIの生成より前に `TrustChain::Core guard;` をインスタンス化し、`guard.verifyToken()` によりオンラインでの証明書バリデーションおよびヌル文字汚染スキャンを実施する。

### 4.2 外部通信インターフェース群
* `ITwitchEventCollector` (Interface)
  * `virtual void connectToTwitch() = 0;`
  * `virtual void disconnectFromTwitch() = 0;`
  * `virtual void requestChatters() = 0;`
  * `virtual void sendShoutout(const QString& toBroadcasterId) = 0;`
  * `virtual void setVipStatus(const QString& targetUserId, bool enable) = 0;`
  * `virtual void setModeratorStatus(const QString& targetUserId, bool enable) = 0;`
  * `virtual void banUser(const QString& targetUserId, int duration, const QString& reason = "") = 0;`
  * `virtual void unbanUser(const QString& targetUserId) = 0;;`
* `ITwitchActionSender` (Interface)
  * `virtual void sendChatMessage(const QString& message) = 0;`
  * `virtual void updateChannelPointStatus(const QString& redemptionId, bool fulfill) = 0;`
* `IObsIntegration` (Interface)
  * `virtual void sendAction(const QString& actionType, const QVariantMap& payload) = 0;`
* `IBouyomiIntegration` (Interface)
  * `virtual void sendText(const QString& text) = 0;`

### 4.3 内部処理モジュールクラス
* `DatabaseManager` / `CommentAnalyzer`
* `ConfigManager`
  * **※設定ファイル入出力時に TransCipher を挟み込んで難読化を行う。**

## 5. 処理シーケンス (データフロー詳細)

**【アプリケーション起動とセキュリティ検証シーケンス】**
1. `main.cpp` が実行される。
2. `TrustChain::Core` が初期化され、`verifyToken()` を呼び出す。
   - ※致命的な不正が検知された場合、ここで安全に自動強制終了する。
3. `MainWindow` のインスタンスが生成される。
4. `TrustChain::QtHelper::applyWatermark` が呼び出され、検証ステータスをもとにUIにウォーターマークの適用可否を判断する。
5. 以降、通常の初期化（AppController等）を行い、イベント割り込み待機ループに入る。

**【コメント受信時の詳細シーケンス】**
1. `TwitchEventCollectorImpl` がWebSocketからJSONを受信。
2. JSONをパースし、構造体 `CommentData` を生成。
3. `QMetaObject::invokeMethod(appController, "onCommentReceived", Qt::QueuedConnection, Q_ARG(CommentData, data))` をコール。
4. `AppController` は非同期（QueuedConnection）で各ワーカースレッドへディスパッチ。
5. 解析やDB保存が別スレッドで並行処理される。

**【視聴者操作およびモデレーションアクションシーケンス】**
1. アプリケーションで「視聴者」タブがアクティブ化される、または10分タイマーが作動する。
2. `AppController` は `m_twitchCollector->requestChatters()` を実行。
3. `TwitchEventCollectorImpl` は `GET /chat/chatters` を呼び出し、レスポンスから `user_id`, `user_name`, `user_badge` を含んだ `ChatterInfo` のリストを構築して `ChattersEvent` を `AppController` へポスト。
4. `AppController` は `MainWindow::updateChattersList` を呼び出し、各アコーディオン内の `QListWidget` に `ChatterRowWidget` を動的バインド。各アイテムの `Qt::UserRole` 等にIDとバッジ情報を格納。
5. ユーザーが視聴者行にホバー/クリックし、表示されたアクションボタンを選択。
   - **シャウトアウト**: `sendShoutout` APIを発行し、2分間のカウントダウンをタイトルバーに表示。
   - **VIP / モデレータトグル**: 確認後（モデレータのみ）、トグル状態に応じて `setVipStatus` / `setModeratorStatus` APIを実行。
   - **タイムアウト**: タイムアウト時間設定ダイアログで時間入力後、確認メッセージ表示。承認後 `banUser(userId, duration)` APIを実行。
   - **BAN**: 確認メッセージ表示後、`banUser(userId, 0)` APIを実行。
6. 各API実行完了時、UIのトグル状態やリスト表示を同期。

## 6. 付属ツール: ログ復号ビューワ (DB Viewer)

暗号化された SQLite データベースを開発者や配信者が確認・メンテナンスするための独立したビューワアプリを同梱します。

* **プロジェクト構成**: メインアプリとは「別の実行ファイル (別EXE)」としてビルド。`DatabaseManager` と `TransCipher-Dist` モジュールをメインアプリと共有する構成とする。
* **画面構成 (ViewerMainWindow.ui)**:
  * **連携起動**: メインアプリ（TwitchCommentManager）からコマンドライン引数で対象DBのパスを受け取り、直接開く構成。
  * **データ表示**: `QTableView` を用い、暗号化された `username_enc` や `message_enc` をメモリ上で復号して平文として一覧表示する（セルは読み取り専用で誤編集を防止）。
  * **削除機能**: 選択したコメントの個別削除機能、および全履歴の全件削除機能を提供する。
