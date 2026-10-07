#pragma once

/// OBS WebSocket v5 protocol constants: envelope op codes, the
/// `eventSubscriptions` bitmask, and the `requestType` / `eventType` wire
/// strings this client uses.
///
/// Request and event names travel as plain strings in the JSON envelope,
/// so they are `const char *` constants here rather than an enum that
/// would need a mapping table (`.name` == wire string is the convention
/// the reference Dart implementation uses too).
///
/// Reference: obsproject/obs-websocket, `docs/generated/protocol.md`.

namespace obs {

/// Envelope op codes - every message is `{"op": <int>, "d": {...}}`.
namespace op {
constexpr int Hello = 0;                ///< OBS -> client, first message
constexpr int Identify = 1;             ///< client -> OBS
constexpr int Identified = 2;           ///< OBS -> client, handshake done
constexpr int Reidentify = 3;
constexpr int Event = 5;                ///< OBS -> client
constexpr int Request = 6;              ///< client -> OBS
constexpr int RequestResponse = 7;      ///< OBS -> client
constexpr int RequestBatch = 8;         ///< client -> OBS
constexpr int RequestBatchResponse = 9; ///< OBS -> client
} // namespace op

/// `eventSubscriptions` bitmask sent in Identify.
///
/// Deliberately excludes `InputVolumeMeters` (1 << 16): those fire once per
/// audio frame per input and would flood the socket with a value this app
/// does not display. `All` is the low bit range that obs-websocket v5
/// defines for the standard categories.
namespace eventSub {
constexpr int General = 1 << 0;
constexpr int Config = 1 << 1;
constexpr int Scenes = 1 << 2;
constexpr int Inputs = 1 << 3;
constexpr int Transitions = 1 << 4;
constexpr int Filters = 1 << 5;
constexpr int Outputs = 1 << 6;
constexpr int SceneItems = 1 << 7;
constexpr int MediaInputs = 1 << 8;
constexpr int Vendors = 1 << 9;
constexpr int Ui = 1 << 10;
constexpr int All = (1 << 11) - 1;
} // namespace eventSub

/// `requestType` values this client sends.
namespace req {
constexpr auto GetVersion = "GetVersion";
constexpr auto GetSceneList = "GetSceneList";
constexpr auto SetCurrentProgramScene = "SetCurrentProgramScene";
constexpr auto GetSceneItemList = "GetSceneItemList";
constexpr auto SetSceneItemEnabled = "SetSceneItemEnabled";
constexpr auto GetInputList = "GetInputList";
constexpr auto GetSpecialInputs = "GetSpecialInputs";
constexpr auto GetInputVolume = "GetInputVolume";
constexpr auto SetInputVolume = "SetInputVolume";
constexpr auto GetInputMute = "GetInputMute";
constexpr auto SetInputMute = "SetInputMute";
constexpr auto GetStreamStatus = "GetStreamStatus";
constexpr auto StartStream = "StartStream";
constexpr auto StopStream = "StopStream";
constexpr auto GetRecordStatus = "GetRecordStatus";
constexpr auto StartRecord = "StartRecord";
constexpr auto StopRecord = "StopRecord";
constexpr auto GetStats = "GetStats";
} // namespace req

/// `eventType` values this client reacts to.
namespace evt {
constexpr auto CurrentProgramSceneChanged = "CurrentProgramSceneChanged";
constexpr auto SceneListChanged = "SceneListChanged";
constexpr auto SceneItemEnableStateChanged = "SceneItemEnableStateChanged";
constexpr auto SceneItemListReindexed = "SceneItemListReindexed";
constexpr auto InputVolumeChanged = "InputVolumeChanged";
constexpr auto InputMuteStateChanged = "InputMuteStateChanged";
constexpr auto InputCreated = "InputCreated";
constexpr auto InputRemoved = "InputRemoved";
constexpr auto InputNameChanged = "InputNameChanged";
constexpr auto StreamStateChanged = "StreamStateChanged";
constexpr auto RecordStateChanged = "RecordStateChanged";
constexpr auto ExitStarted = "ExitStarted";
} // namespace evt

} // namespace obs
