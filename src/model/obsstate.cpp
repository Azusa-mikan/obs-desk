#include "model/obsstate.h"

#include "protocol/obsclient.h"
#include "protocol/obsprotocol.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>

#include <algorithm>

namespace {
constexpr int kPollIntervalMs = 1000;

// Program preview pacing: at least 100 ms between two GetSourceScreenshot
// sends (~10 fps), and a 1 s back-off before retrying a failed screenshot.
// The width is capped at 960 px; OBS scales by width and keeps the aspect.
constexpr int kPreviewMinIntervalMs = 100;
constexpr int kPreviewRetryDelayMs = 1000;
constexpr int kPreviewWidth = 960;

/// Reads a JSON number that may be an integer or a double.
qint64 toInt64(const QJsonValue &value) {
    return static_cast<qint64>(value.toDouble());
}
} // namespace

ObsState::ObsState(ObsClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_pollTimer(new QTimer(this))
    , m_previewTimer(new QTimer(this)) {
    m_pollTimer->setInterval(kPollIntervalMs);
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, &QTimer::timeout, this, &ObsState::sendPreviewFrame);

    connect(m_client, &ObsClient::identified, this, &ObsState::onIdentified);
    connect(m_client, &ObsClient::connectionClosed, this, &ObsState::onDisconnected);
    connect(m_client, &ObsClient::connectionFailed, this, &ObsState::onDisconnected);
    connect(m_client, &ObsClient::responseReceived, this, &ObsState::onResponse);
    connect(m_client, &ObsClient::requestFailed, this, &ObsState::onRequestFailed);
    connect(m_client, &ObsClient::eventReceived, this, &ObsState::onEvent);
    connect(m_pollTimer, &QTimer::timeout, this, &ObsState::onPollTick);
}

// --- accessors -------------------------------------------------------------

QVector<ObsState::AudioInput> ObsState::globalAudioInputs() const {
    QVector<AudioInput> result;
    for (const AudioInput &input : m_audioInputs) {
        if (input.hasAudio && input.global)
            result.append(input);
    }
    return result;
}

QVector<ObsState::AudioInput> ObsState::sceneAudioInputs() const {
    QVector<AudioInput> result;
    for (const AudioInput &input : m_audioInputs) {
        if (input.hasAudio && !input.global)
            result.append(input);
    }
    return result;
}

// --- actions ---------------------------------------------------------------

void ObsState::setCurrentScene(const QString &name) {
    QJsonObject d;
    d[QStringLiteral("sceneName")] = name;
    m_client->sendRequest(obs::req::SetCurrentProgramScene, d);
}

void ObsState::setSceneItemEnabled(int sceneItemId, bool enabled) {
    QJsonObject d;
    d[QStringLiteral("sceneName")] = m_currentScene;
    d[QStringLiteral("sceneItemId")] = sceneItemId;
    d[QStringLiteral("sceneItemEnabled")] = enabled;
    m_client->sendRequest(obs::req::SetSceneItemEnabled, d);
}

void ObsState::setInputVolume(const QString &inputName, double mul) {
    QJsonObject d;
    d[QStringLiteral("inputName")] = inputName;
    d[QStringLiteral("inputVolumeMul")] = mul;
    m_client->sendRequest(obs::req::SetInputVolume, d);
}

void ObsState::setInputMute(const QString &inputName, bool muted) {
    QJsonObject d;
    d[QStringLiteral("inputName")] = inputName;
    d[QStringLiteral("inputMuted")] = muted;
    m_client->sendRequest(obs::req::SetInputMute, d);
}

void ObsState::startStream() {
    m_client->sendRequest(obs::req::StartStream);
}

void ObsState::stopStream() {
    m_client->sendRequest(obs::req::StopStream);
}

void ObsState::startRecord() {
    m_client->sendRequest(obs::req::StartRecord);
}

void ObsState::stopRecord() {
    m_client->sendRequest(obs::req::StopRecord);
}

void ObsState::setPreviewEnabled(bool enabled) {
    if (enabled == m_previewEnabled)
        return;
    m_previewEnabled = enabled;

    if (!enabled) {
        // Closing the window must leave zero network traffic: cancel any
        // pending re-send and forget the in-flight frame so its reply cannot
        // schedule another request.
        m_previewTimer->stop();
        m_previewInFlight = false;
        emit previewStopped();
        return;
    }

    // If already identified, start immediately; otherwise onIdentified() (and
    // the GetSceneList reply that follows it) will kick the loop off.
    if (m_active)
        sendPreviewFrame();
}

// Self-driving loop: a frame is only requested after the previous one came
// back, so the pace is bounded by OBS's response time and the 100 ms floor.
void ObsState::sendPreviewFrame() {
    if (!m_previewEnabled || !m_active || m_currentScene.isEmpty() || m_previewInFlight)
        return;
    m_previewTimer->stop(); // supersede any pending paced/retry send
    m_previewInFlight = true;
    m_previewSince.start();
    QJsonObject d;
    d[QStringLiteral("sourceName")] = m_currentScene;
    d[QStringLiteral("imageFormat")] = QStringLiteral("jpg");
    d[QStringLiteral("imageWidth")] = kPreviewWidth;
    d[QStringLiteral("imageCompressionQuality")] = -1;
    m_client->sendRequest(obs::req::GetSourceScreenshot, d);
}

// --- lifecycle -------------------------------------------------------------

void ObsState::onIdentified() {
    m_active = true;

    m_client->sendRequest(obs::req::GetVersion);
    fetchSceneList();   // also triggers GetSceneItemList for the active scene
    fetchInputList();   // also triggers per-input volume/mute fetches
    m_client->sendRequest(obs::req::GetSpecialInputs); // global audio names
    m_client->sendRequest(obs::req::GetStreamStatus);
    m_client->sendRequest(obs::req::GetRecordStatus);

    m_pollTimer->start();

    // Resume the preview loop if its window is still open. The scene name is
    // not known yet, so sendPreviewFrame() no-ops here and the GetSceneList
    // reply below actually starts it.
    if (m_previewEnabled)
        sendPreviewFrame();
}

void ObsState::onDisconnected() {
    m_active = false;
    m_pollTimer->stop();
    reset();
}

void ObsState::reset() {
    // Stop the preview loop but leave m_previewEnabled alone: the window may
    // still be open and should resume automatically after a reconnect.
    m_previewTimer->stop();
    m_previewInFlight = false;

    m_scenes.clear();
    m_currentScene.clear();
    m_sceneItems.clear();
    m_audioInputs.clear();
    m_stream = Output{};
    m_record = Output{};
    m_stats = Stats{};
    m_obsVersion.clear();
    m_pendingSceneItems.clear();
    m_volumeQueue.clear();
    m_muteQueue.clear();
    m_globalInputNames.clear();

    emit scenesChanged();
    emit currentSceneChanged(m_currentScene);
    emit sceneItemsChanged();
    emit audioInputsChanged();
    emit streamStatusChanged();
    emit recordStatusChanged();
    emit statsChanged();
    emit obsVersionChanged(m_obsVersion);
}

// --- initial/derived fetches ----------------------------------------------

void ObsState::fetchSceneList() {
    m_client->sendRequest(obs::req::GetSceneList);
}

void ObsState::requestSceneItems(const QString &sceneName) {
    m_pendingSceneItems = sceneName;
    QJsonObject d;
    d[QStringLiteral("sceneName")] = sceneName;
    m_client->sendRequest(obs::req::GetSceneItemList, d);
}

void ObsState::fetchInputList() {
    m_client->sendRequest(obs::req::GetInputList);
}

void ObsState::pumpVolumeQueue() {
    if (m_volumeQueue.isEmpty())
        return;
    QJsonObject d;
    d[QStringLiteral("inputName")] = m_volumeQueue.first();
    m_client->sendRequest(obs::req::GetInputVolume, d);
}

void ObsState::pumpMuteQueue() {
    if (m_muteQueue.isEmpty())
        return;
    QJsonObject d;
    d[QStringLiteral("inputName")] = m_muteQueue.first();
    m_client->sendRequest(obs::req::GetInputMute, d);
}

// --- request responses -----------------------------------------------------

void ObsState::onResponse(const QString &requestType, const QJsonObject &responseData) {
    if (requestType == obs::req::GetVersion) {
        m_obsVersion = responseData.value(QStringLiteral("obsWebSocketVersion")).toString();
        emit obsVersionChanged(m_obsVersion);
        return;
    }

    if (requestType == obs::req::GetSceneList) {
        struct TmpScene {
            QString name;
            int index;
        };
        QVector<TmpScene> tmp;
        const QJsonArray scenes = responseData.value(QStringLiteral("scenes")).toArray();
        tmp.reserve(scenes.size());
        for (const QJsonValue &value : scenes) {
            const QJsonObject scene = value.toObject();
            tmp.append({scene.value(QStringLiteral("sceneName")).toString(),
                        scene.value(QStringLiteral("sceneIndex")).toInt()});
        }
        // Wire order is index ascending; display order is index descending so
        // the top-most scene appears first, matching OBS's own scene panel.
        std::sort(tmp.begin(), tmp.end(), [](const TmpScene &a, const TmpScene &b) {
            return a.index > b.index;
        });

        m_scenes.clear();
        m_scenes.reserve(tmp.size());
        for (const TmpScene &scene : tmp)
            m_scenes.append(scene.name);

        const QString current = responseData.value(QStringLiteral("currentProgramSceneName")).toString();
        m_currentScene = current;

        emit scenesChanged();
        emit currentSceneChanged(m_currentScene);

        // (Re)start the preview loop now that the program scene is known. On
        // reconnect this is the first moment the scene name exists, because
        // onIdentified() runs before the GetSceneList reply.
        if (m_previewEnabled)
            sendPreviewFrame();

        if (!m_currentScene.isEmpty())
            requestSceneItems(m_currentScene);
        return;
    }

    if (requestType == obs::req::GetSceneItemList) {
        QVector<SceneItem> items;
        const QJsonArray sceneItems = responseData.value(QStringLiteral("sceneItems")).toArray();
        items.reserve(sceneItems.size());
        for (const QJsonValue &value : sceneItems) {
            const QJsonObject item = value.toObject();
            SceneItem entry;
            entry.id = item.value(QStringLiteral("sceneItemId")).toInt();
            entry.index = item.value(QStringLiteral("sceneItemIndex")).toInt();
            entry.name = item.value(QStringLiteral("sourceName")).toString();
            entry.enabled = item.value(QStringLiteral("sceneItemEnabled")).toBool();
            entry.isGroup = item.value(QStringLiteral("isGroup")).toBool();
            entry.inputKind = item.value(QStringLiteral("inputKind")).toString();
            items.append(entry);
        }
        // Same display-order rule as scenes: top-most item first.
        std::sort(items.begin(), items.end(), [](const SceneItem &a, const SceneItem &b) {
            return a.index > b.index;
        });

        // Ignore replies for a scene the user already navigated away from.
        if (m_pendingSceneItems == m_currentScene) {
            m_sceneItems = items;
            emit sceneItemsChanged();
        }
        return;
    }

    if (requestType == obs::req::GetInputList) {
        // Keep previously known volume/mute for inputs that survive a refresh.
        QHash<QString, AudioInput> previous;
        for (const AudioInput &input : m_audioInputs)
            previous.insert(input.name, input);

        QVector<AudioInput> inputs;
        const QJsonArray array = responseData.value(QStringLiteral("inputs")).toArray();
        inputs.reserve(array.size());
        for (const QJsonValue &value : array) {
            const QString name = value.toObject().value(QStringLiteral("inputName")).toString();
            if (name.isEmpty())
                continue;
            AudioInput input;
            input.name = name;
            if (previous.contains(name))
                input = previous.value(name);
            inputs.append(input);
        }
        // Apply the global grouping from GetSpecialInputs. Doing it here (rather
        // than only in the GetSpecialInputs handler) makes the two replies
        // order-independent: whichever arrives last still produces the right
        // grouping.
        for (AudioInput &input : inputs)
            input.global = m_globalInputNames.contains(input.name);
        m_audioInputs = inputs;
        emit audioInputsChanged();

        // Fetch volume and mute for every input, one in flight at a time.
        m_volumeQueue.clear();
        m_muteQueue.clear();
        for (const AudioInput &input : m_audioInputs) {
            m_volumeQueue.append(input.name);
            m_muteQueue.append(input.name);
        }
        pumpVolumeQueue();
        pumpMuteQueue();
        return;
    }

    if (requestType == obs::req::GetSpecialInputs) {
        // Six fields, each a name or null: the global (desktop/mic) inputs.
        // Record their names and re-tag every known input so the panel can
        // split Global from Scene, whichever reply arrived first.
        static const char *const kFields[] = {"desktop1", "desktop2", "mic1",
                                              "mic2",     "mic3",     "mic4"};
        m_globalInputNames.clear();
        for (const char *field : kFields) {
            const QString name = responseData.value(QString::fromLatin1(field)).toString();
            if (!name.isEmpty())
                m_globalInputNames.insert(name);
        }
        for (AudioInput &input : m_audioInputs)
            input.global = m_globalInputNames.contains(input.name);
        emit audioInputsChanged();
        return;
    }

    if (requestType == obs::req::GetInputVolume) {
        // Replies are matched positionally to the FIFO queue (see header).
        if (m_volumeQueue.isEmpty()) {
            return;
        }
        const QString name = m_volumeQueue.takeFirst();
        const double mul = responseData.value(QStringLiteral("inputVolumeMul")).toDouble(1.0);
        for (AudioInput &input : m_audioInputs) {
            if (input.name == name) {
                // A successful GetInputVolume proves the source supports audio
                // (the 604 branch in onRequestFailed() is the negative case).
                input.capabilityKnown = true;
                input.hasAudio = true;
                input.volumeMul = mul;
                emit audioInputsChanged();
                break;
            }
        }
        pumpVolumeQueue();
        return;
    }

    if (requestType == obs::req::GetInputMute) {
        if (m_muteQueue.isEmpty()) {
            return;
        }
        const QString name = m_muteQueue.takeFirst();
        const bool muted = responseData.value(QStringLiteral("inputMuted")).toBool();
        for (AudioInput &input : m_audioInputs) {
            if (input.name == name) {
                input.muted = muted;
                emit audioInputsChanged();
                break;
            }
        }
        pumpMuteQueue();
        return;
    }

    if (requestType == obs::req::GetStreamStatus) {
        m_stream.active = responseData.value(QStringLiteral("outputActive")).toBool();
        m_stream.durationMs = toInt64(responseData.value(QStringLiteral("outputDuration")));
        m_stream.bytes = toInt64(responseData.value(QStringLiteral("outputBytes")));
        emit streamStatusChanged();
        return;
    }

    if (requestType == obs::req::GetRecordStatus) {
        m_record.active = responseData.value(QStringLiteral("outputActive")).toBool();
        m_record.durationMs = toInt64(responseData.value(QStringLiteral("outputDuration")));
        m_record.bytes = toInt64(responseData.value(QStringLiteral("outputBytes")));
        emit recordStatusChanged();
        return;
    }

    if (requestType == obs::req::GetStats) {
        m_stats.fps = responseData.value(QStringLiteral("activeFps")).toDouble();
        m_stats.renderTimeMs = responseData.value(QStringLiteral("averageFrameRenderTime")).toDouble();
        m_stats.cpuUsage = responseData.value(QStringLiteral("cpuUsage")).toDouble();
        m_stats.memoryUsage = toInt64(responseData.value(QStringLiteral("memoryUsage")));
        m_stats.availableDiskSpace = toInt64(responseData.value(QStringLiteral("availableDiskSpace")));
        m_stats.skippedFrames = responseData.value(QStringLiteral("outputSkippedFrames")).toInt();
        m_stats.totalFrames = responseData.value(QStringLiteral("outputTotalFrames")).toInt();
        emit statsChanged();
        return;
    }

    if (requestType == obs::req::GetSourceScreenshot) {
        const QString imageData = responseData.value(QStringLiteral("imageData")).toString();
        // `imageData` is a data URI such as
        // "data:image/jpeg;base64,<BASE64>": take the payload after the comma.
        // Defensively treat the whole string as base64 when there is none.
        const int comma = imageData.indexOf(QLatin1Char(','));
        const QByteArray encoded = (comma >= 0 ? imageData.mid(comma + 1) : imageData).toLatin1();
        emit previewFrameReady(QByteArray::fromBase64(encoded));

        m_previewInFlight = false;
        if (!m_previewEnabled)
            return;

        // ~10 fps floor: send the next frame now if the interval already
        // elapsed, otherwise wait out the remainder with the single-shot timer.
        const qint64 elapsed = m_previewSince.elapsed();
        if (elapsed >= kPreviewMinIntervalMs)
            sendPreviewFrame();
        else
            m_previewTimer->start(kPreviewMinIntervalMs - static_cast<int>(elapsed));
        return;
    }

    // Set* acknowledgements and anything else need no model update.
}

void ObsState::onRequestFailed(const QString &requestType, int code, const QString &comment) {
    // A per-input probe finishes on failure too: obs-websocket answers every
    // request, so the FIFO must always advance. Without this, one non-audio
    // source (604 below) would stall all later volume/mute fetches behind it.
    if (requestType == obs::req::GetInputVolume) {
        if (m_volumeQueue.isEmpty())
            return;
        const QString name = m_volumeQueue.takeFirst();
        if (code == 604) {
            // 604 = InvalidResourceState: "the specified input does not support
            // audio". This is the expected reply for video-only sources, so mark
            // the capability instead of surfacing an error.
            for (AudioInput &input : m_audioInputs) {
                if (input.name == name) {
                    input.capabilityKnown = true;
                    input.hasAudio = false;
                    emit audioInputsChanged();
                    break;
                }
            }
        }
        pumpVolumeQueue();
        return;
    }

    if (requestType == obs::req::GetInputMute) {
        if (m_muteQueue.isEmpty())
            return;
        m_muteQueue.takeFirst();
        pumpMuteQueue();
        return;
    }

    if (requestType == obs::req::GetSourceScreenshot) {
        // The preview is high-frequency, so a transient failure (for example
        // the scene is mid-switch) must not spam errorOccurred() every frame.
        // Back off and quietly retry while the window is still open.
        m_previewInFlight = false;
        if (m_previewEnabled)
            m_previewTimer->start(kPreviewRetryDelayMs);
        return;
    }

    emit errorOccurred(tr("%1 failed (code %2): %3").arg(requestType).arg(code).arg(comment));
}

void ObsState::onPollTick() {
    if (!m_active)
        return;
    m_client->sendRequest(obs::req::GetStats);
    m_client->sendRequest(obs::req::GetStreamStatus);
    m_client->sendRequest(obs::req::GetRecordStatus);
}

// --- events ----------------------------------------------------------------

void ObsState::onEvent(const QString &eventType, const QJsonObject &eventData) {
    if (eventType == obs::evt::CurrentProgramSceneChanged) {
        m_currentScene = eventData.value(QStringLiteral("sceneName")).toString();
        emit currentSceneChanged(m_currentScene);
        requestSceneItems(m_currentScene);
        return;
    }

    if (eventType == obs::evt::SceneListChanged) {
        fetchSceneList();
        return;
    }

    if (eventType == obs::evt::SceneItemEnableStateChanged) {
        const QString sceneName = eventData.value(QStringLiteral("sceneName")).toString();
        if (sceneName != m_currentScene)
            return;
        const int itemId = eventData.value(QStringLiteral("sceneItemId")).toInt();
        const bool enabled = eventData.value(QStringLiteral("sceneItemEnabled")).toBool();
        for (SceneItem &item : m_sceneItems) {
            if (item.id == itemId) {
                item.enabled = enabled;
                emit sceneItemsChanged();
                break;
            }
        }
        return;
    }

    if (eventType == obs::evt::SceneItemListReindexed) {
        const QString sceneName = eventData.value(QStringLiteral("sceneName")).toString();
        if (sceneName != m_currentScene)
            return;
        const QJsonArray reindexed = eventData.value(QStringLiteral("sceneItems")).toArray();
        for (const QJsonValue &value : reindexed) {
            const QJsonObject entry = value.toObject();
            const int itemId = entry.value(QStringLiteral("sceneItemId")).toInt();
            const int newIndex = entry.value(QStringLiteral("sceneItemIndex")).toInt();
            for (SceneItem &item : m_sceneItems) {
                if (item.id == itemId) {
                    item.index = newIndex;
                    break;
                }
            }
        }
        std::sort(m_sceneItems.begin(), m_sceneItems.end(), [](const SceneItem &a, const SceneItem &b) {
            return a.index > b.index;
        });
        emit sceneItemsChanged();
        return;
    }

    if (eventType == obs::evt::InputVolumeChanged) {
        const QString name = eventData.value(QStringLiteral("inputName")).toString();
        const double mul = eventData.value(QStringLiteral("inputVolumeMul")).toDouble(1.0);
        for (AudioInput &input : m_audioInputs) {
            if (input.name == name && !qFuzzyCompare(input.volumeMul, mul)) {
                input.volumeMul = mul;
                emit audioInputsChanged();
                break;
            }
        }
        return;
    }

    if (eventType == obs::evt::InputMuteStateChanged) {
        const QString name = eventData.value(QStringLiteral("inputName")).toString();
        const bool muted = eventData.value(QStringLiteral("inputMuted")).toBool();
        for (AudioInput &input : m_audioInputs) {
            if (input.name == name && input.muted != muted) {
                input.muted = muted;
                emit audioInputsChanged();
                break;
            }
        }
        return;
    }

    if (eventType == obs::evt::InputCreated || eventType == obs::evt::InputRemoved
        || eventType == obs::evt::InputNameChanged) {
        fetchInputList();
        return;
    }

    if (eventType == obs::evt::StreamStateChanged) {
        m_stream.active = eventData.value(QStringLiteral("outputActive")).toBool();
        emit streamStatusChanged();
        return;
    }

    if (eventType == obs::evt::RecordStateChanged) {
        m_record.active = eventData.value(QStringLiteral("outputActive")).toBool();
        emit recordStatusChanged();
        return;
    }

    if (eventType == obs::evt::ExitStarted) {
        emit obsExiting();
        return;
    }
}
