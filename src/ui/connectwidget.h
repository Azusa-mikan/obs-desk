#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/// Connection form: host, port, optional password and a status/error line.
///
/// Prefills from (and writes back to) AppSettings. Emits connectRequested()
/// when the user activates Connect; the owner is responsible for the actual
/// connection and for calling setBusy()/showError()/setStatus().
class ConnectWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConnectWidget(QWidget *parent = nullptr);

    void setBusy(bool busy);
    void showError(const QString &message);
    void setStatus(const QString &message);

signals:
    void connectRequested(const QString &host, quint16 port, const QString &password);

private slots:
    void onConnectClicked();
    void onShowPasswordToggled(bool show);

private:
    void loadSettings();
    void saveSettings();

    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_rememberCheck = nullptr;
    QCheckBox *m_showPasswordCheck = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusLabel = nullptr;
};
