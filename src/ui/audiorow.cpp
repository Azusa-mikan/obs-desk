#include "ui/audiorow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

#include <algorithm>

namespace {
// Slider range is 0-2000 "percent" units; mul = value / 100.0.
constexpr int kSliderMax = 2000;
constexpr double kSliderScale = 100.0;
} // namespace

AudioRow::AudioRow(const QString &inputName, QWidget *parent)
    : QWidget(parent)
    , m_inputName(inputName) {
    m_nameLabel = new QLabel(inputName, this);
    m_nameLabel->setMinimumWidth(120);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, kSliderMax);
    m_slider->setValue(static_cast<int>(kSliderScale));
    m_slider->setToolTip(tr("Volume"));

    m_valueLabel = new QLabel(QStringLiteral("100%"), this);
    m_valueLabel->setMinimumWidth(48);
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_muteButton = new QToolButton(this);
    m_muteButton->setCheckable(true);
    m_muteButton->setText(QStringLiteral("M"));
    m_muteButton->setToolTip(tr("Mute / unmute"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(m_nameLabel);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_muteButton);

    connect(m_slider, &QSlider::valueChanged, this, &AudioRow::onSliderValueChanged);
    connect(m_muteButton, &QToolButton::toggled, this, &AudioRow::onMuteToggled);
}

void AudioRow::setVolume(double mul) {
    const int value = std::clamp(static_cast<int>(mul * kSliderScale + 0.5), 0, kSliderMax);
    // Block signals so a state refresh does not bounce back as a SetInputVolume.
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(value);
    updateVolumeLabel(value);
}

void AudioRow::setMuted(bool muted) {
    const QSignalBlocker blocker(m_muteButton);
    m_muteButton->setChecked(muted);
    m_muteButton->setText(muted ? QStringLiteral("Muted") : QStringLiteral("M"));
}

void AudioRow::onSliderValueChanged(int value) {
    updateVolumeLabel(value);
    emit volumeChanged(m_inputName, value / kSliderScale);
}

void AudioRow::onMuteToggled(bool muted) {
    m_muteButton->setText(muted ? QStringLiteral("Muted") : QStringLiteral("M"));
    emit muteToggled(m_inputName, muted);
}

void AudioRow::updateVolumeLabel(int value) {
    m_valueLabel->setText(QString::number(value) + QStringLiteral("%"));
}
