#include "ui/mainwindow.h"

#include "model/obsstate.h"
#include "protocol/obsclient.h"
#include "ui/connectwidget.h"
#include "ui/dashboardwidget.h"

#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new ObsClient(this))
    , m_state(new ObsState(m_client, this)) {
    setWindowTitle(tr("OBS Control"));
    resize(1000, 700);

    m_connectWidget = new ConnectWidget(this);
    m_dashboard = new DashboardWidget(this);
    m_dashboard->setState(m_state);
    m_dashboard->setLocked(true);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_connectWidget); // index 0
    m_stack->addWidget(m_dashboard);     // index 1
    setCentralWidget(m_stack);

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    connect(m_connectWidget, &ConnectWidget::connectRequested, m_client, &ObsClient::connectToObs);
    connect(m_client, &ObsClient::socketConnected, this, &MainWindow::onSocketConnected);
    connect(m_client, &ObsClient::identified, this, &MainWindow::onIdentified);
    connect(m_client, &ObsClient::connectionClosed, this, &MainWindow::onConnectionClosed);
    connect(m_client, &ObsClient::connectionFailed, this, &MainWindow::onConnectionFailed);
    connect(m_state, &ObsState::obsVersionChanged, this, &MainWindow::onObsVersionChanged);
    connect(m_state, &ObsState::obsExiting, this, &MainWindow::onObsExiting);
    connect(m_state, &ObsState::errorOccurred, this, &MainWindow::onModelError);

    updateStatusBar();
}

void MainWindow::onSocketConnected() {
    m_connectWidget->setStatus(tr("Connected \u2014 handshaking\u2026"));
    updateStatusBar();
}

void MainWindow::onIdentified() {
    m_connected = true;
    m_dashboard->setLocked(false);
    m_stack->setCurrentWidget(m_dashboard);
    m_connectWidget->setBusy(false);
    updateStatusBar();
}

void MainWindow::onConnectionClosed() {
    m_connected = false;
    m_obsVersion.clear();
    m_dashboard->setLocked(true);
    m_stack->setCurrentWidget(m_connectWidget);
    m_connectWidget->setBusy(false);
    if (m_exiting)
        m_connectWidget->setStatus(tr("OBS is exiting \u2014 disconnected."));
    else
        m_connectWidget->setStatus(tr("Disconnected."));
    m_exiting = false;
    updateStatusBar();
}

void MainWindow::onConnectionFailed(const QString &reason) {
    m_connected = false;
    m_obsVersion.clear();
    m_exiting = false;
    m_dashboard->setLocked(true);
    m_stack->setCurrentWidget(m_connectWidget);
    m_connectWidget->setBusy(false);
    m_connectWidget->showError(reason);
    updateStatusBar();
}

void MainWindow::onObsVersionChanged(const QString &version) {
    m_obsVersion = version;
    updateStatusBar();
}

void MainWindow::onObsExiting() {
    // OBS announced ExitStarted; tear the session down and go back to Connect.
    m_exiting = true;
    m_client->disconnectFromObs();
}

void MainWindow::onModelError(const QString &message) {
    statusBar()->showMessage(message, 5000);
}

void MainWindow::updateStatusBar() {
    QString text;
    if (m_connected) {
        text = m_obsVersion.isEmpty() ? tr("Connected")
                                      : tr("Connected \u2014 obs-websocket %1").arg(m_obsVersion);
    } else {
        text = tr("Not connected");
    }
    m_statusLabel->setText(text);
}
