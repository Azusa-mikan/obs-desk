#include "ui/connectwidget.h"

#include "appsettings.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectWidget::ConnectWidget(QWidget *parent)
    : QWidget(parent) {
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("(empty if OBS has no password)"));

    m_showPasswordCheck = new QCheckBox(tr("Show"), this);
    m_rememberCheck = new QCheckBox(tr("Remember password"), this);

    auto *passwordRow = new QHBoxLayout;
    passwordRow->addWidget(m_passwordEdit, 1);
    passwordRow->addWidget(m_showPasswordCheck);

    auto *form = new QFormLayout;
    form->addRow(tr("Host"), m_hostEdit);
    form->addRow(tr("Port"), m_portSpin);
    form->addRow(tr("Password"), passwordRow);

    m_connectButton = new QPushButton(tr("Connect"), this);
    m_connectButton->setDefault(true);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addLayout(form);
    layout->addWidget(m_rememberCheck);
    layout->addWidget(m_connectButton);
    layout->addWidget(m_statusLabel);
    layout->addStretch(2);

    connect(m_connectButton, &QPushButton::clicked, this, &ConnectWidget::onConnectClicked);
    connect(m_showPasswordCheck, &QCheckBox::toggled, this, &ConnectWidget::onShowPasswordToggled);
    // Enter in either text field submits the form.
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &ConnectWidget::onConnectClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &ConnectWidget::onConnectClicked);

    loadSettings();
}

void ConnectWidget::loadSettings() {
    m_hostEdit->setText(AppSettings::host());
    m_portSpin->setValue(AppSettings::port());
    m_rememberCheck->setChecked(AppSettings::rememberPassword());
    m_passwordEdit->setText(AppSettings::password());
}

void ConnectWidget::saveSettings() {
    AppSettings::setHost(m_hostEdit->text().trimmed());
    AppSettings::setPort(static_cast<quint16>(m_portSpin->value()));
    const bool remember = m_rememberCheck->isChecked();
    AppSettings::setRememberPassword(remember);
    // Only the opted-in password is persisted; clear it otherwise so a stale
    // plain-text secret is not left behind in QSettings.
    AppSettings::setPassword(remember ? m_passwordEdit->text() : QString());
}

void ConnectWidget::setBusy(bool busy) {
    m_hostEdit->setEnabled(!busy);
    m_portSpin->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_rememberCheck->setEnabled(!busy);
    m_showPasswordCheck->setEnabled(!busy);
    m_connectButton->setEnabled(!busy);
    m_connectButton->setText(busy ? tr("Connecting\u2026") : tr("Connect"));
}

void ConnectWidget::showError(const QString &message) {
    m_statusLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));
    m_statusLabel->setText(message);
}

void ConnectWidget::setStatus(const QString &message) {
    m_statusLabel->setStyleSheet(QString());
    m_statusLabel->setText(message);
}

void ConnectWidget::onConnectClicked() {
    if (!m_connectButton->isEnabled())
        return;
    saveSettings();
    setBusy(true);
    setStatus(tr("Connecting\u2026"));
    emit connectRequested(m_hostEdit->text().trimmed(),
                          static_cast<quint16>(m_portSpin->value()),
                          m_passwordEdit->text());
}

void ConnectWidget::onShowPasswordToggled(bool show) {
    m_passwordEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}
