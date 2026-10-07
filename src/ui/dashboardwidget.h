#pragma once

#include <QHash>
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;
class AudioRow;
class ObsState;

/// Control surface shown once the handshake completes.
///
/// Sections: Scenes, Scene Items, Audio, Output (stream/record) and Stats.
/// All state flows through ObsState - the widget never talks to ObsClient
/// directly. It is bound with setState() and locked while disconnected.
class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    void setState(ObsState *state);

    /// Disables the whole control surface (used when the socket drops).
    void setLocked(bool locked);

private slots:
    void onScenesChanged();
    void onCurrentSceneChanged(const QString &name);
    void onSceneItemsChanged();
    void onAudioInputsChanged();
    void onStreamStatusChanged();
    void onRecordStatusChanged();
    void onStatsChanged();

    void onSceneClicked(QListWidgetItem *item);
    void onSceneItemChanged(QListWidgetItem *item);

private:
    QWidget *buildScenesSection();
    QWidget *buildSceneItemsSection();
    QWidget *buildAudioSection();
    QWidget *buildOutputSection();
    QWidget *buildStatsSection();
    void updateOutputButtons();

    ObsState *m_state = nullptr;

    QListWidget *m_sceneList = nullptr;
    QListWidget *m_sceneItemList = nullptr;

    QVBoxLayout *m_globalAudioLayout = nullptr;
    QVBoxLayout *m_sceneAudioLayout = nullptr;
    QLabel *m_globalAudioPlaceholder = nullptr;
    QLabel *m_sceneAudioPlaceholder = nullptr;
    QHash<QString, AudioRow *> m_audioRows;

    QPushButton *m_streamButton = nullptr;
    QPushButton *m_recordButton = nullptr;
    QLabel *m_streamStatusLabel = nullptr;
    QLabel *m_recordStatusLabel = nullptr;

    QLabel *m_fpsLabel = nullptr;
    QLabel *m_cpuLabel = nullptr;
    QLabel *m_memoryLabel = nullptr;
    QLabel *m_droppedLabel = nullptr;
    QLabel *m_bitrateLabel = nullptr;
    QLabel *m_durationLabel = nullptr;

    // True while repopulating the scene-item list, so programmatic check-state
    // changes are not mistaken for user clicks.
    bool m_updatingSceneItems = false;
};
