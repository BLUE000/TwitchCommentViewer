# Twitchコメント解析ツール 詳細設計書

ウォーターフォール Vモデルにおける「詳細設計（モジュール内部設計・DBスキーマ・クラス設計・シーケンス）」を定義します。

---

## 1. セキュリティ設計・ビルド構成 (TrustChain導入)

本アプリケーションの「出自証明」および「改ざん防止・再配布対策」として、**TrustChain** を導入します。

* **CMake構成**: 
  - プロジェクトのサブディレクトリとして `TrustChain` を追加（`add_subdirectory`）し、ビルド時に自動で出自確認およびトークン埋め込みを実施する。
  - メインアプリのほか、**付属ツール（DB Viewer）**も別EXEとして同一プロジェクト（CMake）内でビルド可能にする。
  - `trustchain_credentials.cmake` を利用し、GitHubリポジトリおよびTransCipherサーバーのURL、秘密鍵を安全に管理（Git管理外とする）。
* **第3層プロテクト（バイナリ透かし）**:
  - アプリ（`.exe`）完成後、ビルドプロセスに同梱される `BinMarkManager` を利用して物理的な権利情報（ウォーターマーク）をバイナリに埋め込むフローを採用する。

---

## 2. 画面設計 (UI Layout)

`MainWindow.ui` の構成要素：

* **基盤**: `QMainWindow`
* **セキュリティ対応**: 
  - アプリ起動時に `TrustChain::QtHelper::applyWatermark(&w, status)` を適用。改ざんやオフライン状態が検知された場合は、タイトルバーに `(Custom Build)` を付与し、ステータスバーに権利表記を強制表示する。
* **メイン領域**: `QTabWidget`（以下の4タブ構成）

### 2.1 ビューワタブ（旧コメントタブ）
* `tableViewComments` (QTableView): コメントをリアルタイム表示。右クリックでコンテキストメニューを起動し、ピン留め操作が可能。
* **チャットアナウンスエリア (最下部)**:
  - 横並びレイアウト (`QHBoxLayout`) を配置。
  - `comboAnnouncementColor` (QComboBox): アナウンス背景色を選択。
    - 選択肢: 通常 (`primary`), 青 (`blue`), 緑 (`green`), オレンジ (`orange`), 紫 (`purple`)
  - `lineAnnouncementText` (QLineEdit): メッセージ入力欄（最大500文字の文字数制限プレースホルダーあり）。
  - `btnSendAnnouncement` (QPushButton): アナウンス送信実行ボタン。

### 2.2 視聴者タブ
* 手動「更新」ボタン、および最終更新時間ラベル。
* `QScrollArea` と 7つのアコーディオン（すべて/ストリーマー/モデレーター/VIP/アーティスト/チャットボット/一般）。
* 各アコーディオンは `QPushButton`（トグル）と `QListWidget` で構成。
* ストリーマーを除く視聴者行にカスタム行ウィジェット（`ChatterRowWidget`）を配置し、マウスホバー時または行選択時に右端にアクションボタンを表示。
  - `[シャウトアウト] [VIP] [モデレータ]  (余白)  [タイムアウト] [BAN]` の配置と余白制御。
  - VIP・モデレータ・BANはトグル動作とし、状態を表現。
  - タイムアウト時は時間指定ダイアログを起動。
  - モデレータ追加/削除、タイムアウト決定後、BAN時には実行確認メッセージ（`QMessageBox`）を表示。
  - シャウトアウト時は、2分間のクールタイム表示（ウィンドウタイトルバーでのリアルタイムカウントダウン）を制御。
  - 視聴者が複数の属性を持つ場合（例: Nightbotはモデレーターかつチャットボット）、それぞれの `QListWidget` に重複して登録・表示する。

### 2.3 解析タブ
* 左側縦タブ（`QTabWidget`、ポジション: West）でサブタブを切り替え可能。
  - **解析ログサブタブ**: `QTextBrowser`（トレンド・スパム等の検知ログ表示）
  - **コメント集計サブタブ**:
    - 表示グラフ切り替え (`comboGraphType`): 総コメント数推移 / ユーザーごとの推移
    - 時間粒度切り替え (`comboGranularity`): 自動 / 5秒 / 10秒 / 30秒 / 1分 / 5分
    - 表示数切り替え (`comboTopN`): TOP 3 / TOP 5 / TOP 10 / 全員表示
    - コメント総数表示ラベル (`labelTotalComments`)
    - ユーザー別コメント数ランキング (`tableUserStats`)
    - 折れ線グラフ表示エリア (`chartView`): `QtCharts` を用いた描画。ウィンドウリサイズ時に `resizeEvent` をトリガーとし、X軸・Y軸の目盛り間隔を動的に再計算して描画領域に適合させる（ウィンドウサイズ自体の自動調整は行わない）。

### 2.4 設定タブ
* Twitch接続設定（OAuth連携開始ボタン、チャンネル名入力など）。
* 読み上げ（TTS）設定: 棒読みちゃんポート番号、音量、自動起動設定。
* **読み上げ除外ユーザー設定**:
  - `editTtsIgnoreUsers` (QLineEdit): 読み上げを無視するユーザー名のカンマ区切り入力欄。
* **BOT除外設定**:
  - `editBotUsers` (QLineEdit): BOT判定するユーザー名のカンマ区切り入力欄。
* 権利・ライセンス表記（TrustChain・Qt等のライセンスを記載）。

---

## 3. データ保護と資格情報管理設計

### 3.1 難読化
* プライバシー保護およびセキュリティの観点から、DBのみならずシステムが生成する中間ファイル（ログの一時キャッシュ、設定ファイル、集計用の一時エクスポート等）について、`TransCipher-Dist` を利用して難読化して保存します。

### 3.2 セキュアOAuthトークン管理 (Windows DPAPI)
* ユーザーのOAuthアクセストークンは、Windows環境においてはWindowsデータ保護API（DPAPI: `CryptProtectData` および `CryptUnprotectData`）を使用してローカルユーザーアカウントに紐づく鍵で暗号化します。
* 暗号化されたトークンデータは、実行バイナリと同ディレクトリ内の `tokens.enc` ファイルにバイナリ保存されます。
* Windows以外のOSでは、`TransCipher-Dist` による難読化方式にフォールバックします。

### 3.3 データベーススキーマ設計 (SQLite)
個人特定が可能なカラムも同様に `TransCipher-Dist` により難読化して保存します。

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

---

## 4. クラス設計・インターフェース設計

### 4.1 アプリケーション初期化層 (main.cpp)
* `main()` 関数内において、UIの生成より前に `TrustChain::Core guard;` をインスタンス化し、`guard.verifyToken()` によりオンラインでの証明書バリデーションおよびヌル文字汚染スキャンを実施する。

### 4.2 外部通信インターフェース群
#### `ITwitchEventCollector` (Interface)
* `virtual void connectToTwitch() = 0;`
* `virtual void disconnectFromTwitch() = 0;`
* `virtual void requestChatters() = 0;`
* `virtual void sendShoutout(const QString& toBroadcasterId) = 0;`
* `virtual void setVipStatus(const QString& targetUserId, bool enable) = 0;`
* `virtual void setModeratorStatus(const QString& targetUserId, bool enable) = 0;`
* `virtual void banUser(const QString& targetUserId, int duration, const QString& reason = "") = 0;`
* `virtual void unbanUser(const QString& targetUserId) = 0;`
* `virtual void pinChatMessage(const QString& messageId, int durationSeconds) = 0;`
* `virtual void unpinChatMessage(const QString& messageId) = 0;`
* `virtual void sendAnnouncement(const QString& message, const QString& color) = 0;` // 新規追加

#### `TwitchEventCollectorImpl` (ITwitchEventCollectorの実装クラス)
* QWebSocket と QNetworkAccessManager を保持し、EventSubの購読およびHelix REST APIを呼び出す。
* **新規API追加点**:
  - `sendAnnouncement(const QString& message, const QString& color)`:
    - エンドポイント: `POST https://api.twitch.tv/helix/chat/announcements?broadcaster_id={broadcaster_id}&moderator_id={moderator_id}`
    - ペイロード JSON: `{ "message": "...", "color": "..." }`
    - 送信時ヘッダー: Client-Id, Authorization (Bearer), Content-Type (application/json)

### 4.3 内部処理モジュールクラス
#### `ConfigManager`
* `loadConfig()` / `saveConfig()`: 設定をJSON形式で難読化保存・復元。
* `saveToken(const QString& token)` / `loadToken()`: DPAPI / TransCipherを用いた暗号化トークンの保存とロード。
* `getBotUsers()` / `setBotUsers(const QStringList& bots)`: BOT除外判定用リストの取得・更新。
* `getTtsIgnoreUsers()` / `setTtsIgnoreUsers(const QStringList& users)`: 読み上げ無視ユーザーリストの取得・更新。

#### `BouyomiIntegrationImpl`
* 棒読みちゃんへのTCPソケット経由でのメッセージ送出クラス。
* 読み上げの際、渡されたユーザー名が `ConfigManager::getTtsIgnoreUsers()` または `ConfigManager::getBotUsers()` (自動Bot除外) に合致する場合はソケット送信を行わず早期リターンする。

---

## 5. 処理シーケンス (データフロー詳細)

### 5.1 アプリケーション起動とセキュリティ検証
1. `main.cpp` 起動 → `TrustChain::Core::verifyToken()` 実行。不正時はプロセス強制終了。
2. `MainWindow` インスタンス生成 → `applyWatermark()` 適用。
3. `ConfigManager::loadConfig()` および `ConfigManager::loadToken()` で設定とトークンをロード。

### 5.2 コメント受信と読み上げ（TTS除外フィルター含む）
1. `TwitchEventCollectorImpl` がWebSocketからチャットメッセージを受信。
2. JSONをパースし、`CommentEvent` を構築して `AppController::onCommentReceived` へ `invokeMethod(Qt::QueuedConnection)` で通知。
3. `AppController` は非同期で `CommentAnalyzer`（解析）と `DatabaseManager`（DB保存）に処理をディスパッチ。
4. `AppController` は `ConfigManager` を用いて読み上げ除外判定を行う。
   - 発言者が `ttsIgnoreUsers` または `botUsers` に含まれる場合: 読み上げ処理をスキップ。
   - 含まれない場合: `BouyomiIntegration::sendText()` を呼び出して棒読みちゃんへ送出。
5. UIスレッドで `MainWindow::addCommentToView()` を呼び出して表示を更新。

### 5.3 チャットピン留めシーケンス
1. ユーザーがコメント表示行を右クリックし、「📌 チャット上部にピン留めする」から時間（例: 5分）を選択。
2. `MainWindow` は `m_controller` に向けて `onPinCommentRequested(messageId, 300)` を `invokeMethod` で通知。
3. `AppController` はワーカースレッドの `TwitchEventCollectorImpl::pinChatMessage()` を呼び出す。
4. `TwitchEventCollectorImpl` は `POST /helix/chat/pins?broadcaster_id=xxx&moderator_id=xxx` を実行（JSON: `{"message_id":"...","duration_seconds":300}`）。
5. 成功後、UIでピン留め中状態を記憶し、再度右クリックされた場合には「❌ ピン留めを解除」を表示。

### 5.4 チャットアナウンス送信シーケンス
1. ユーザーがコメントタブ最下部のアナウンス入力欄にテキストを入力し、プルダウンから背景色（例: `blue`）を選択して「アナウンス送信」をクリック。
2. `MainWindow` は `lineAnnouncementText` の内容を読み出し、`m_controller` へ `onSendAnnouncementRequested(message, color)` を `invokeMethod` で通知。
3. `AppController` は `TwitchEventCollectorImpl::sendAnnouncement(message, color)` を非同期に呼び出す。
4. `TwitchEventCollectorImpl` は、`POST /helix/chat/announcements` に対して以下のリクエストを送信する。
   - Query parameters: `broadcaster_id` (ストリーマーID), `moderator_id` (モデレーターID = ストリーマーID)
   - Header: `Authorization: Bearer <token>`, `Client-Id: <client_id>`, `Content-Type: application/json`
   - JSON Body: `{"message": "入力されたテキスト", "color": "blue"}`
5. APIレスポンス受信後、成功時は送信テキスト欄をクリアし、失敗時はレスポンスエラー内容をステータスバーまたはログへ通知する。

### 5.5 グラフの目盛り（Nice Steps）算出アルゴリズム
`MainWindow::updateChartDisplay()` における、画面サイズに適応した綺麗な目盛り間隔を求めるアルゴリズム。

#### X軸 (時間軸) のステップ算出
1. 表示領域の幅 `chartWidth` を取得し、目標分割数 `targetDivisionsX = max(2, chartWidth / 150)` を算出。
2. データの時間幅 `durationMs` を `targetDivisionsX` で割り、目安間隔 `targetIntervalX` を求める。
3. 規定の時間ステップ配列（5s, 10s, 30s, 1m, 2m, 5m, 10m, 15m, 30m, 1h...）の中から、`targetIntervalX` 以上の最小値となるステップ値 `niceIntervalX` を選択する。
4. X軸の表示範囲（最小値・最大値）を `niceIntervalX` の倍数に丸め、`tickCountX = (最大値 - 最小値) / niceIntervalX + 1` を設定する。

#### Y軸 (コメント数) のステップ算出
1. 表示領域の高さ `chartHeight` を取得し、目標分割数 `targetDivisionsY = max(2, chartHeight / 50)` を算出。
2. 最大コメント数 `maxCount` を `targetDivisionsY` で割り、目安ステップ `targetStep` を求める。
3. `targetStep` に応じて、以下のキリの良い値 `niceStep` を選択する。
   - `targetStep <= 1`: 1
   - `targetStep <= 2`: 2
   - `targetStep <= 5`: 5
   - `targetStep <= 10`: 10
   - `targetStep <= 20`: 20
   - `targetStep <= 25`: 25
   - `targetStep <= 50`: 50
   - `targetStep <= 100`: 100
   - `targetStep <= 250`: 250
   - `targetStep <= 500`: 500
   - それ以上は、10の累乗倍を基準に `10^k`, `2 * 10^k`, `2.5 * 10^k`, `5 * 10^k` を判定。
4. Y軸の表示範囲を `niceStep` の倍数に丸めて `[0, roundedMax]` とし、`tickCountY = roundedMax / niceStep + 1` を設定する。
