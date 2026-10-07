#pragma once

#include <QByteArray>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QTimer;
class ObsState;

/// Standalone top-level window showing the OBS Program scene as a live,
/// ~10 fps JPEG preview.
///
/// The window owns no polling itself: it is a thin view on ObsState's preview
/// loop. Showing the window enables the loop; closing it disables the loop, so
/// a closed window costs zero network traffic. It never touches Studio Mode or
/// the Preview scene.
class PreviewWindow : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWindow(ObsState *state, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onFrameReady(const QByteArray &imageBytes);
    void onFpsTick();

private:
    void rescaleFrame();
    void updateStatusText();

    ObsState *m_state = nullptr;
    QLabel *m_imageLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    // Last decoded frame, kept so resizeEvent() can re-scale it without
    // waiting for the next screenshot.
    QPixmap m_frame;
    bool m_hasFrame = false;

    // Crude frames-per-second counter, sampled once a second.
    QTimer *m_fpsTimer = nullptr;
    int m_framesThisSecond = 0;
    int m_lastFps = 0;
};
