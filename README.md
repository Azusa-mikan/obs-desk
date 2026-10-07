# OBS Control

A small Qt 6 Widgets desktop app that remote-controls a local or LAN
**OBS Studio** instance over the **obs-websocket v5** protocol.

Target user: someone on a computer (especially Linux) who wants a lightweight
remote for OBS — switch scenes, toggle source visibility, adjust/mute audio,
start & stop streaming/recording, and watch live stats.

## Features

- Connect to OBS over WebSocket v5, with optional password authentication.
- **Scenes**: list sorted top-layer-first; click to switch the program scene.
- **Scene Items**: checkbox per source to show/hide it (groups are shown as a
  single item in this MVP).
- **Audio**: per-input volume slider (0–100%, matching OBS's 0 dB fader) and
  a mute toggle. Only inputs confirmed to support audio are listed, and they
  are grouped into **Global** and **Scene** sections like OBS's own mixer.
- **Output**: start/stop streaming and recording.
- **Stats**: FPS, CPU, memory, dropped frames, average stream bitrate and
  stream time, refreshed once per second.
- Live updates wired to obs-websocket events (scene changes, item visibility,
  volume/mute, stream/record state, input add/remove).
- Remembers host/port and optionally the password.

## Dependencies

- A C++17 compiler (GCC/Clang)
- CMake ≥ 3.16 and Ninja (or Make)
- Qt 6.3+ with the modules:
  - `qt6-base` (Core, Gui, Widgets, Network)
  - `qt6-websockets`

On Arch Linux:

```sh
sudo pacman -S base-devel cmake ninja qt6-base qt6-websockets
```

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Run:

```sh
./build/obs_control
```

## Enabling WebSocket in OBS

1. In OBS Studio open **Tools → WebSocket Server Settings**.
2. Tick **Enable WebSocket server**.
3. Note the **Server Port** (default `4455`).
4. If you enable authentication, note the **Server Password**.
5. Connect from the app using host `127.0.0.1` for the same machine, or the
   machine's LAN IP for a remote OBS.

## Configuration

The last-used connection is stored with `QSettings`
(`~/.config/obs_control/obs_control.conf` on Linux):

- Host and port are always saved.
- The password is saved **only** when **Remember password** is checked, and it
  is stored in **plain text** — see Limitations.

## Usage

1. Fill in Host / Port / Password and click **Connect** (or press Enter).
2. On success the app switches to the control view.
3. Use the Scenes and Scene Items lists, the Audio sliders, and the Output
   buttons. Stats update once per second.
4. If the connection drops or authentication fails, the app returns to the
   Connect page with the reason shown.

## Limitations (MVP scope)

- No automatic reconnect.
- No preview/screenshot of the program output.
- No transitions, filters, or Studio Mode support.
- Groups are treated as ordinary scene items (their children are not listed).
- If **Remember password** is checked, the password is kept in plain text in
  `QSettings`; there is no OS secret store integration.
- Only the top-level scene items of the current scene are managed.
- Audio capability is probed per input: sources without audio (images, text,
  colour sources, ...) are hidden rather than shown with a dead control. The
  volume/mute of each audio input is fetched one at a time after the input
  list changes.

## Disclaimer

This application is an independent project and is **not affiliated with or
endorsed by the OBS Project**. "OBS Studio" is a trademark of its respective
owners.
