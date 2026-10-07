#include "ui/previewwindow.h"

#include "model/obsstate.h"

#include <QCloseEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

PreviewWindow::PreviewWindow(ObsState *state, QWidget *parent)
    : QWidget(parent)
    , m_state(state) {
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("Program Preview"));
    resize(960, 540); // 16:9, matching the 960 px screenshot cap

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setText(tr("No preview yet"));
    // Let the label fill the window without its pixmap forcing a size: the
    // frame is scaled to the label, never the other way round.
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_statusLabel = new QLabel(tr("Waiting for OBS\u2026"), this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_imageLabel, 1);
    layout->addWidget(m_statusLabel);

    m_fpsTimer = new QTimer(this);
    m_fpsTimer->setInterval(1000);
    connect(m_fpsTimer, &QTimer::timeout, this, &PreviewWindow::onFpsTick);
    m_fpsTimer->start();

    if (m_state)
        connect(m_state, &ObsState::previewFrameReady, this, &PreviewWindow::onFrameReady);
}

void PreviewWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Start requesting frames only while the window is actually visible.
    if (m_state)
        m_state->setPreviewEnabled(true);
}

void PreviewWindow::closeEvent(QCloseEvent *event) {
    // Closing stops the preview loop: the model sends nothing further.
    if (m_state)
        m_state->setPreviewEnabled(false);
    QWidget::closeEvent(event);
    event->accept();
}

void PreviewWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rescaleFrame();
}

void PreviewWindow::onFrameReady(const QByteArray &imageBytes) {
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageBytes)) {
        // Ignore a corrupt frame and keep showing the previous one.
        return;
    }
    m_frame = pixmap;
    m_hasFrame = true;
    ++m_framesThisSecond;
    rescaleFrame();
    updateStatusText();
}

void PreviewWindow::rescaleFrame() {
    if (!m_hasFrame)
        return;
    m_imageLabel->setPixmap(
        m_frame.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PreviewWindow::updateStatusText() {
    if (!m_hasFrame) {
        if (m_state && m_state->currentScene().isEmpty())
            m_statusLabel->setText(tr("No scene"));
        else
            m_statusLabel->setText(tr("Waiting for OBS\u2026"));
        return;
    }
    m_statusLabel->setText(
        tr("%1 \u00d7 %2  \u2014  %3 fps").arg(m_frame.width()).arg(m_frame.height()).arg(m_lastFps));
}

void PreviewWindow::onFpsTick() {
    m_lastFps = m_framesThisSecond;
    m_framesThisSecond = 0;
    updateStatusText();
}
