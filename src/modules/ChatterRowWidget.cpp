#include "ChatterRowWidget.h"
#include <QSpacerItem>

ChatterRowWidget::ChatterRowWidget(const QString& userId, const QString& userName, const QStringList& userBadges, QWidget* parent)
    : QWidget(parent)
    , m_userId(userId)
    , m_userName(userName)
    , m_userBadges(userBadges)
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(8);

    m_lblName = new QLabel(userName, this);
    mainLayout->addWidget(m_lblName);

    mainLayout->addStretch(); // 左寄せにするためユーザー名とボタン群の間に可変ストレッチを挟む

    m_actionWidget = new QWidget(this);
    QHBoxLayout* actionLayout = new QHBoxLayout(m_actionWidget);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(4);

    m_btnShoutout = new QPushButton("SO", m_actionWidget); // "Shoutout"
    m_btnShoutout->setToolTip("シャウトアウトを送信");
    m_btnShoutout->setFixedWidth(32);

    m_btnVip = new QPushButton("VIP", m_actionWidget);
    m_btnVip->setCheckable(true);
    m_btnVip->setToolTip("VIP権限の付与/除外");
    m_btnVip->setFixedWidth(36);

    m_btnMod = new QPushButton("MOD", m_actionWidget);
    m_btnMod->setCheckable(true);
    m_btnMod->setToolTip("モデレータ権限の付与/除外");
    m_btnMod->setFixedWidth(42);

    actionLayout->addWidget(m_btnShoutout);
    actionLayout->addWidget(m_btnVip);
    actionLayout->addWidget(m_btnMod);

    // 誤操作防止の全角スペース2つ分相当の余白 (約30px)
    actionLayout->addSpacing(30);

    m_btnTimeout = new QPushButton("TO", m_actionWidget); // "Timeout"
    m_btnTimeout->setToolTip("タイムアウト（時間指定）");
    m_btnTimeout->setFixedWidth(32);

    m_btnBan = new QPushButton("BAN", m_actionWidget);
    m_btnBan->setCheckable(true);
    m_btnBan->setToolTip("永久BAN/アンBAN");
    m_btnBan->setFixedWidth(38);

    actionLayout->addWidget(m_btnTimeout);
    actionLayout->addWidget(m_btnBan);

    mainLayout->addWidget(m_actionWidget);

    m_actionWidget->hide(); // 初期状態は非表示

    // ボタンのシグナル接続
    connect(m_btnShoutout, &QPushButton::clicked, this, [this]() {
        emit shoutoutClicked(m_userId, m_userName);
    });
    connect(m_btnVip, &QPushButton::toggled, this, [this](bool checked) {
        emit vipToggled(m_userId, m_userName, checked);
    });
    connect(m_btnMod, &QPushButton::toggled, this, [this](bool checked) {
        emit moderatorToggled(m_userId, m_userName, checked);
    });
    connect(m_btnTimeout, &QPushButton::clicked, this, [this]() {
        emit timeoutClicked(m_userId, m_userName);
    });
    connect(m_btnBan, &QPushButton::toggled, this, [this](bool checked) {
        emit banToggled(m_userId, m_userName, checked);
    });

    // 初期状態のバッジに応じたボタン表示切り替え
    updateButtonsFromBadges(userBadges);
}

void ChatterRowWidget::setSelectedState(bool selected) {
    m_selected = selected;
    updateVisibility();
}

void ChatterRowWidget::setShoutoutButtonEnabled(bool enabled) {
    if (m_btnShoutout) {
        m_btnShoutout->setEnabled(enabled);
    }
}

void ChatterRowWidget::updateButtonsFromBadges(const QStringList& badges) {
    // vipトグルの状態を設定
    m_btnVip->blockSignals(true);
    m_btnVip->setChecked(badges.contains("vip"));
    m_btnVip->blockSignals(false);

    // modトグルの状態を設定
    m_btnMod->blockSignals(true);
    m_btnMod->setChecked(badges.contains("moderator") || badges.contains("broadcaster"));
    m_btnMod->blockSignals(false);

    // 自分自身（broadcaster）や他のモデレーターに対する保護
    if (badges.contains("broadcaster")) {
        m_btnShoutout->setEnabled(false);
        m_btnVip->setEnabled(false);
        m_btnMod->setEnabled(false);
        m_btnTimeout->setEnabled(false);
        m_btnBan->setEnabled(false);
    } else if (badges.contains("moderator")) {
        m_btnVip->setEnabled(false); // モデレータはVIPになれない
    }
}

void ChatterRowWidget::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_hovered = true;
    updateVisibility();
}

void ChatterRowWidget::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_hovered = false;
    updateVisibility();
}

void ChatterRowWidget::updateVisibility() {
    if (m_userBadges.contains("broadcaster")) {
        m_actionWidget->hide();
        return;
    }
    if (m_hovered || m_selected) {
        m_actionWidget->show();
    } else {
        m_actionWidget->hide();
    }
}
