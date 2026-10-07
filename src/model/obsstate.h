#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QJsonObject;
class QTimer;
class ObsClient;

/// Central, UI-facing state model.
///
/// Subscribes to an `ObsClient` and turns the raw protocol into readable
/// properties plus change signals. It also owns the periodic polling and the
/// one-shot fetch sequences (scene items, per-input volume/mute).
///
/// List ordering contract: both scene and scene-item lists are kept in
/// *display* order - index descending - so the top-most layer/scene is first,
/// matching OBS's own panel. The wire order is index ascending.
class ObsState : public QObject {
    Q_OBJECT

public:
    struct SceneItem {
        int id = 0;
        int index = 0;
        QString name;
        bool enabled = false;
        bool isGroup = false;
        QString inputKind;
    };

    struct AudioInput {
        QString name;
        double volumeMul = 1.0;
        bool muted = false;
    };

    struct Output {
        bool active = false;
        qint64 durationMs = 0;
        qint64 bytes = 0;
    };

    struct Stats {
        double fps = 0.0;
        double renderTimeMs = 0.0;
        double cpuUsage = 0.0;
        qint64 memoryUsage = 0;
        qint64 availableDiskSpace = 0;
        int skippedFrames = 0;
        int totalFrames = 0;
    };

    explicit ObsState(ObsClient *client, QObject *parent = nullptr);

    const QVector<QString> &scenes() const { return m_scenes; }
    QString currentScene() const { return m_currentScene; }
    const QVector<SceneItem> &sceneItems() const { return m_sceneItems; }
    const QVector<AudioInput> &audioInputs() const { return m_audioInputs; }
    Output streamStatus() const { return m_stream; }
    Output recordStatus() const { return m_record; }
    Stats stats() const { return m_stats; }
    QString obsVersion() const { return m_obsVersion; }

    // --- actions -----------------------------------------------------------
    void setCurrentScene(const QString &name);
    void setSceneItemEnabled(int sceneItemId, bool enabled);
    void setInputVolume(const QString &inputName, double mul);
    void setInputMute(const QString &inputName, bool muted);
    void startStream();
    void stopStream();
    void startRecord();
    void stopRecord();

signals:
    void scenesChanged();
    void currentSceneChanged(const QString &name);
    void sceneItemsChanged();
    void audioInputsChanged();
    void streamStatusChanged();
    void recordStatusChanged();
    void statsChanged();
    void obsVersionChanged(const QString &version);
    /// OBS announced it is shutting down.
    void obsExiting();
    /// A request failed or the model hit an unexpected value.
    void errorOccurred(const QString &message);

private slots:
    void onIdentified();
    void onDisconnected();
    void onResponse(const QString &requestType, const QJsonObject &responseData);
    void onRequestFailed(const QString &requestType, int code, const QString &comment);
    void onEvent(const QString &eventType, const QJsonObject &eventData);
    void onPollTick();

private:
    void reset();
    void requestSceneItems(const QString &sceneName);
    void fetchSceneList();
    void fetchInputList();
    void pumpVolumeQueue();
    void pumpMuteQueue();

    ObsClient *m_client = nullptr;
    QTimer *m_pollTimer = nullptr;

    QVector<QString> m_scenes; // display order (index desc)
    QString m_currentScene;
    QVector<SceneItem> m_sceneItems; // display order (index desc)
    QVector<AudioInput> m_audioInputs;
    Output m_stream;
    Output m_record;
    Stats m_stats;
    QString m_obsVersion;

    // The scene a GetSceneItemList reply belongs to. obs-websocket does not
    // echo the scene name in the response, so it is tracked here and the
    // reply is ignored if the user has since switched scenes.
    QString m_pendingSceneItems;

    // Per-input volume/mute replies are mapped back to inputs positionally:
    // only one request of each type is in flight at a time (FIFO), which is
    // safe because obs-websocket answers in order. This avoids needing the
    // requestId in the response signal.
    QStringList m_volumeQueue;
    QStringList m_muteQueue;

    // Guards against reacting to our own slider write-back loops is handled in
    // the UI layer via QSignalBlocker; the model simply stores values.
    bool m_active = false;
};
