#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QEnterEvent>

class ChatterRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatterRowWidget(const QString& userId, const QString& userName, const QString& userBadge, QWidget* parent = nullptr);
    ~ChatterRowWidget() override = default;

    QString userId() const { return m_userId; }
    QString userName() const { return m_userName; }
    QString userBadge() const { return m_userBadge; }

    void setSelectedState(bool selected);
    void setShoutoutButtonEnabled(bool enabled);
    void updateButtonsFromBadge(const QString& badge);

signals:
    void shoutoutClicked(const QString& userId, const QString& userName);
    void vipToggled(const QString& userId, const QString& userName, bool checked);
    void moderatorToggled(const QString& userId, const QString& userName, bool checked);
    void timeoutClicked(const QString& userId, const QString& userName);
    void banToggled(const QString& userId, const QString& userName, bool checked);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QString m_userId;
    QString m_userName;
    QString m_userBadge;

    bool m_hovered = false;
    bool m_selected = false;

    QLabel* m_lblName = nullptr;
    QWidget* m_actionWidget = nullptr;
    QPushButton* m_btnShoutout = nullptr;
    QPushButton* m_btnVip = nullptr;
    QPushButton* m_btnMod = nullptr;
    QPushButton* m_btnTimeout = nullptr;
    QPushButton* m_btnBan = nullptr;

    void updateVisibility();
};
