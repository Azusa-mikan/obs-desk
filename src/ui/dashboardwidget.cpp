#include "ui/dashboardwidget.h"

#include "model/obsstate.h"
#include "ui/audiorow.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>

namespace {
// Scene-item id is stashed on the list item so checkbox changes can be mapped
// back to a protocol request.
constexpr int kSceneItemIdRole = Qt::UserRole + 1;

QString formatDuration(qint64 milliseconds) {
    if (milliseconds < 0)
        milliseconds = 0;
    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatMemory(qint64 bytes) {
    return QStringLiteral("%1 MiB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 0);
}
} // namespace

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent) {
    auto *scenesRow = new QHBoxLayout;
    scenesRow->addWidget(buildScenesSection(), 1);
    scenesRow->addWidget(buildSceneItemsSection(), 1);

    auto *outputStatsRow = new QHBoxLayout;
    outputStatsRow->addWidget(buildOutputSection(), 1);
    outputStatsRow->addWidget(buildStatsSection(), 1);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(scenesRow, 3);
    layout->addWidget(buildAudioSection(), 2);
    layout->addLayout(outputStatsRow, 2);
}

QWidget *DashboardWidget::buildScenesSection() {
    auto *box = new QGroupBox(tr("Scenes"), this);
    m_sceneList = new QListWidget(box);
    m_sceneList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *layout = new QVBoxLayout(box);
    layout->addWidget(m_sceneList);

    connect(m_sceneList, &QListWidget::itemClicked, this, &DashboardWidget::onSceneClicked);
    return box;
}

QWidget *DashboardWidget::buildSceneItemsSection() {
    auto *box = new QGroupBox(tr("Scene Items"), this);
    m_sceneItemList = new QListWidget(box);

    auto *layout = new QVBoxLayout(box);
    layout->addWidget(m_sceneItemList);

    connect(m_sceneItemList, &QListWidget::itemChanged, this, &DashboardWidget::onSceneItemChanged);
    return box;
}

QWidget *DashboardWidget::buildAudioSection() {
    auto *box = new QGroupBox(tr("Audio"), this);

    auto *scroll = new QScrollArea(box);
    scroll->setWidgetResizable(true);

    auto *container = new QWidget(scroll);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(2, 2, 2, 2);

    QFont sectionFont = container->font();
    sectionFont.setBold(true);

    auto *globalTitle = new QLabel(tr("GLOBAL"), container);
    globalTitle->setFont(sectionFont);
    containerLayout->addWidget(globalTitle);

    m_globalAudioPlaceholder = new QLabel(tr("No global audio sources"), container);
    containerLayout->addWidget(m_globalAudioPlaceholder);

    auto *globalContainer = new QWidget(container);
    m_globalAudioLayout = new QVBoxLayout(globalContainer);
    m_globalAudioLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(globalContainer);

    auto *sceneTitle = new QLabel(tr("SCENE"), container);
    sceneTitle->setFont(sectionFont);
    containerLayout->addWidget(sceneTitle);

    m_sceneAudioPlaceholder = new QLabel(tr("No audio sources in this scene"), container);
    containerLayout->addWidget(m_sceneAudioPlaceholder);

    auto *sceneContainer = new QWidget(container);
    m_sceneAudioLayout = new QVBoxLayout(sceneContainer);
    m_sceneAudioLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(sceneContainer);

    containerLayout->addStretch(1);
    scroll->setWidget(container);

    auto *layout = new QVBoxLayout(box);
    layout->addWidget(scroll);
    return box;
}

QWidget *DashboardWidget::buildOutputSection() {
    auto *box = new QGroupBox(tr("Output"), this);

    m_streamButton = new QPushButton(tr("Start Streaming"), box);
    m_recordButton = new QPushButton(tr("Start Recording"), box);
    m_streamStatusLabel = new QLabel(tr("Stream: idle"), box);
    m_recordStatusLabel = new QLabel(tr("Record: idle"), box);

    auto *layout = new QVBoxLayout(box);
    layout->addWidget(m_streamButton);
    layout->addWidget(m_streamStatusLabel);
    layout->addWidget(m_recordButton);
    layout->addWidget(m_recordStatusLabel);
    layout->addStretch(1);

    connect(m_streamButton, &QPushButton::clicked, this, [this]() {
        if (!m_state)
            return;
        if (m_state->streamStatus().active)
            m_state->stopStream();
        else
            m_state->startStream();
    });
    connect(m_recordButton, &QPushButton::clicked, this, [this]() {
        if (!m_state)
            return;
        if (m_state->recordStatus().active)
            m_state->stopRecord();
        else
            m_state->startRecord();
    });
    return box;
}

QWidget *DashboardWidget::buildStatsSection() {
    auto *box = new QGroupBox(tr("Stats"), this);

    m_fpsLabel = new QLabel(tr("FPS: -"), box);
    m_cpuLabel = new QLabel(tr("CPU: -"), box);
    m_memoryLabel = new QLabel(tr("Memory: -"), box);
    m_droppedLabel = new QLabel(tr("Dropped frames: -"), box);
    m_bitrateLabel = new QLabel(tr("Stream bitrate: -"), box);
    m_durationLabel = new QLabel(tr("Stream time: -"), box);

    auto *layout = new QVBoxLayout(box);
    layout->addWidget(m_fpsLabel);
    layout->addWidget(m_cpuLabel);
    layout->addWidget(m_memoryLabel);
    layout->addWidget(m_droppedLabel);
    layout->addWidget(m_bitrateLabel);
    layout->addWidget(m_durationLabel);
    layout->addStretch(1);
    return box;
}

void DashboardWidget::setState(ObsState *state) {
    m_state = state;
    if (!m_state)
        return;

    connect(m_state, &ObsState::scenesChanged, this, &DashboardWidget::onScenesChanged);
    connect(m_state, &ObsState::currentSceneChanged, this, &DashboardWidget::onCurrentSceneChanged);
    connect(m_state, &ObsState::sceneItemsChanged, this, &DashboardWidget::onSceneItemsChanged);
    connect(m_state, &ObsState::audioInputsChanged, this, &DashboardWidget::onAudioInputsChanged);
    connect(m_state, &ObsState::streamStatusChanged, this, &DashboardWidget::onStreamStatusChanged);
    connect(m_state, &ObsState::recordStatusChanged, this, &DashboardWidget::onRecordStatusChanged);
    connect(m_state, &ObsState::statsChanged, this, &DashboardWidget::onStatsChanged);

    // Seed from whatever the model already holds.
    onScenesChanged();
    onCurrentSceneChanged(m_state->currentScene());
    onSceneItemsChanged();
    onAudioInputsChanged();
    onStreamStatusChanged();
    onRecordStatusChanged();
    onStatsChanged();
}

void DashboardWidget::setLocked(bool locked) {
    setEnabled(!locked);
}

// --- scenes ----------------------------------------------------------------

void DashboardWidget::onScenesChanged() {
    if (!m_state)
        return;
    m_sceneList->clear();
    for (const QString &name : m_state->scenes())
        m_sceneList->addItem(name);
    onCurrentSceneChanged(m_state->currentScene());
}

void DashboardWidget::onCurrentSceneChanged(const QString &name) {
    for (int row = 0; row < m_sceneList->count(); ++row) {
        QListWidgetItem *item = m_sceneList->item(row);
        QFont font = item->font();
        const bool isCurrent = (item->text() == name);
        font.setBold(isCurrent);
        item->setFont(font);
        if (isCurrent)
            m_sceneList->setCurrentItem(item);
    }
}

void DashboardWidget::onSceneClicked(QListWidgetItem *item) {
    if (!m_state || !item)
        return;
    if (item->text() == m_state->currentScene())
        return;
    m_state->setCurrentScene(item->text());
}

// --- scene items -----------------------------------------------------------

void DashboardWidget::onSceneItemsChanged() {
    if (!m_state)
        return;

    m_updatingSceneItems = true;
    m_sceneItemList->clear();
    for (const ObsState::SceneItem &item : m_state->sceneItems()) {
        auto *listItem = new QListWidgetItem(item.name);
        listItem->setData(kSceneItemIdRole, item.id);
        listItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        if (item.isGroup)
            listItem->setToolTip(tr("Group \u2014 shown as a single item in this MVP"));
        m_sceneItemList->addItem(listItem);
    }
    m_updatingSceneItems = false;
}

void DashboardWidget::onSceneItemChanged(QListWidgetItem *item) {
    if (!m_state || !item || m_updatingSceneItems)
        return;
    const int id = item->data(kSceneItemIdRole).toInt();
    m_state->setSceneItemEnabled(id, item->checkState() == Qt::Checked);
}

// --- audio -----------------------------------------------------------------

void DashboardWidget::onAudioInputsChanged() {
    if (!m_state)
        return;

    // Only confirmed-audio inputs are exposed, split into the two sections.
    const QVector<ObsState::AudioInput> globalInputs = m_state->globalAudioInputs();
    const QVector<ObsState::AudioInput> sceneInputs = m_state->sceneAudioInputs();

    // Target set of confirmed-audio input names across both sections.
    QSet<QString> target;
    for (const ObsState::AudioInput &input : globalInputs)
        target.insert(input.name);
    for (const ObsState::AudioInput &input : sceneInputs)
        target.insert(input.name);

    // Drop rows for inputs that disappeared.
    for (auto it = m_audioRows.begin(); it != m_audioRows.end();) {
        if (target.contains(it.key())) {
            ++it;
            continue;
        }
        AudioRow *row = it.value();
        if (auto *rowLayout = qobject_cast<QVBoxLayout *>(row->parentWidget()->layout()))
            rowLayout->removeWidget(row);
        row->deleteLater();
        it = m_audioRows.erase(it);
    }

    // Create/refresh rows in their section, then backfill values.
    auto refreshSection = [this](const QVector<ObsState::AudioInput> &inputs, QVBoxLayout *layout) {
        QWidget *section = layout->parentWidget();
        for (const ObsState::AudioInput &input : inputs) {
            AudioRow *row = m_audioRows.value(input.name, nullptr);
            if (!row) {
                row = new AudioRow(input.name, this);
                connect(row, &AudioRow::volumeChanged, m_state, &ObsState::setInputVolume);
                connect(row, &AudioRow::muteToggled, m_state, &ObsState::setInputMute);
                m_audioRows.insert(input.name, row);
                layout->addWidget(row);
            } else if (row->parentWidget() != section) {
                // The input was re-tagged global/scene after its row was built
                // (GetSpecialInputs can arrive after GetInputList): move it.
                if (auto *oldLayout = qobject_cast<QVBoxLayout *>(row->parentWidget()->layout()))
                    oldLayout->removeWidget(row);
                layout->addWidget(row);
            }
            // setVolume/setMuted use QSignalBlocker internally, so this
            // write-back does not bounce out as a SetInputVolume/SetInputMute.
            row->setVolume(input.volumeMul);
            row->setMuted(input.muted);
        }
    };
    refreshSection(globalInputs, m_globalAudioLayout);
    refreshSection(sceneInputs, m_sceneAudioLayout);

    m_globalAudioPlaceholder->setVisible(globalInputs.isEmpty());
    m_sceneAudioPlaceholder->setVisible(sceneInputs.isEmpty());
}

// --- output / stats --------------------------------------------------------

void DashboardWidget::updateOutputButtons() {
    if (!m_state)
        return;
    const bool streaming = m_state->streamStatus().active;
    const bool recording = m_state->recordStatus().active;

    m_streamButton->setText(streaming ? tr("Stop Streaming") : tr("Start Streaming"));
    m_recordButton->setText(recording ? tr("Stop Recording") : tr("Start Recording"));
    m_streamStatusLabel->setText(streaming ? tr("Stream: live") : tr("Stream: idle"));
    m_recordStatusLabel->setText(recording ? tr("Record: recording") : tr("Record: idle"));
}

void DashboardWidget::onStreamStatusChanged() {
    if (!m_state)
        return;
    updateOutputButtons();

    const ObsState::Output stream = m_state->streamStatus();
    if (stream.durationMs > 0) {
        // Average bitrate over the whole session: kbit/s = 8 * bytes / durationMs.
        const double kbitPerSecond = 8.0 * static_cast<double>(stream.bytes) / static_cast<double>(stream.durationMs);
        m_bitrateLabel->setText(tr("Stream bitrate: %1 kbit/s").arg(kbitPerSecond, 0, 'f', 0));
    } else {
        m_bitrateLabel->setText(tr("Stream bitrate: -"));
    }
    m_durationLabel->setText(stream.active ? tr("Stream time: %1").arg(formatDuration(stream.durationMs))
                                           : tr("Stream time: -"));
}

void DashboardWidget::onRecordStatusChanged() {
    if (!m_state)
        return;
    updateOutputButtons();

    const ObsState::Output record = m_state->recordStatus();
    m_recordStatusLabel->setText(record.active
                                     ? tr("Record: recording (%1)").arg(formatDuration(record.durationMs))
                                     : tr("Record: idle"));
}

void DashboardWidget::onStatsChanged() {
    if (!m_state)
        return;
    const ObsState::Stats stats = m_state->stats();
    m_fpsLabel->setText(tr("FPS: %1").arg(stats.fps, 0, 'f', 1));
    m_cpuLabel->setText(tr("CPU: %1%").arg(stats.cpuUsage, 0, 'f', 1));
    m_memoryLabel->setText(tr("Memory: %1").arg(formatMemory(stats.memoryUsage)));
    m_droppedLabel->setText(tr("Dropped frames: %1 / %2").arg(stats.skippedFrames).arg(stats.totalFrames));
}
