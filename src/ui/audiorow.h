#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QToolButton;

/// One audio input row: name, volume slider and a mute toggle.
///
/// The slider works in "percent" units (0-100) so that `mul = value / 100.0`
/// and the label can show `value%` directly (mul 1.0 -> "100%").
/// `setVolume()` / `setMuted()` write state back from the model without
/// re-emitting the change signals (they use QSignalBlocker), so the UI can be
/// refreshed from OBS events without creating a feedback loop.
class AudioRow : public QWidget {
    Q_OBJECT

public:
    explicit AudioRow(const QString &inputName, QWidget *parent = nullptr);

    QString inputName() const { return m_inputName; }

    /// Fills the widgets from model state; does not emit volumeChanged/muteToggled.
    void setVolume(double mul);
    void setMuted(bool muted);

signals:
    void volumeChanged(const QString &inputName, double mul);
    void muteToggled(const QString &inputName, bool muted);

private slots:
    void onSliderValueChanged(int value);
    void onMuteToggled(bool muted);

private:
    void updateVolumeLabel(int value);

    QString m_inputName;
    QLabel *m_nameLabel = nullptr;
    QSlider *m_slider = nullptr;
    QLabel *m_valueLabel = nullptr;
    QToolButton *m_muteButton = nullptr;
};
