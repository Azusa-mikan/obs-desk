#pragma once

#include <QMainWindow>
#include <QString>

class QLabel;
class QStackedWidget;
class ConnectWidget;
class DashboardWidget;
class PreviewWindow;
class ObsClient;
class ObsState;

/// Application shell: a stacked Connect / Dashboard view plus a status bar.
///
/// Owns the ObsClient and the ObsState model and wires them to the two pages:
/// page 0 is the connection form, page 1 the control surface.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onSocketConnected();
    void onIdentified();
    void onConnectionClosed();
    void onConnectionFailed(const QString &reason);
    void onObsVersionChanged(const QString &version);
    void onObsExiting();
    void onModelError(const QString &message);
    void onPreviewRequested();

private:
    void updateStatusBar();

    ObsClient *m_client = nullptr;
    ObsState *m_state = nullptr;

    QStackedWidget *m_stack = nullptr;
    ConnectWidget *m_connectWidget = nullptr;
    DashboardWidget *m_dashboard = nullptr;
    PreviewWindow *m_preview = nullptr; // created on first request
    QLabel *m_statusLabel = nullptr;

    QString m_obsVersion;
    bool m_connected = false;
    bool m_exiting = false;
};
