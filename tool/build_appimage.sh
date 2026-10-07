#!/usr/bin/env bash
#
# Build a self-contained AppImage for OBS Desk.
#
# Usage: tool/build_appimage.sh [VERSION]
#
# VERSION defaults to 0.1.0. The script builds the project in Release mode,
# assembles an AppDir, downloads linuxdeploy + its Qt plugin on first run, and
# produces build-appimage/OBS_Desk-<VERSION>-x86_64.AppImage.
#
# It targets a normal, non-root user with network access; no sudo is used.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-0.1.0}"

WORK="$REPO_ROOT/build-appimage"
APPDIR="$WORK/AppDir"
TOOLS_DIR="$WORK/tools"
BUILD_DIR="$WORK/build"

APPDIR_BIN="$APPDIR/usr/bin/obs_desk"
APPDIR_DESKTOP="$APPDIR/usr/share/applications/obs_desk.desktop"
APPDIR_ICON="$APPDIR/usr/share/icons/hicolor/256x256/apps/obs_desk.png"

echo "==> Repo root : $REPO_ROOT"
echo "==> Version   : $VERSION"

# --- 1. Locate Qt6's qmake ------------------------------------------------

QMAKE=""
if [ -n "${QT_ROOT_DIR:-}" ] && [ -x "$QT_ROOT_DIR/bin/qmake" ]; then
    QMAKE="$QT_ROOT_DIR/bin/qmake"
    echo "==> Using qmake from QT_ROOT_DIR: $QMAKE"
elif command -v qmake6 >/dev/null 2>&1; then
    QMAKE="$(command -v qmake6)"
    echo "==> Using qmake6: $QMAKE"
elif command -v qmake >/dev/null 2>&1; then
    QMAKE="$(command -v qmake)"
    echo "==> WARNING: falling back to 'qmake'; it may NOT be Qt6." >&2
else
    echo "ERROR: could not find a Qt qmake (looked for \$QT_ROOT_DIR/bin/qmake, qmake6, qmake)." >&2
    exit 1
fi
export QMAKE

# --- 2. Build in Release mode and assemble the AppDir ---------------------

echo "==> Building (Release) ..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

echo "==> Assembling AppDir at $APPDIR"
rm -rf "$APPDIR"
mkdir -p \
    "$(dirname "$APPDIR_BIN")" \
    "$(dirname "$APPDIR_DESKTOP")" \
    "$(dirname "$APPDIR_ICON")"
cp "$BUILD_DIR/obs_desk" "$APPDIR_BIN"
cp "$REPO_ROOT/assets/obs_desk.desktop" "$APPDIR_DESKTOP"
cp "$REPO_ROOT/assets/icons/obs_desk.png" "$APPDIR_ICON"
chmod +x "$APPDIR_BIN"

# --- 3. Fetch linuxdeploy + Qt plugin -------------------------------------

LD_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LD_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

mkdir -p "$TOOLS_DIR"
if [ ! -x "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" ]; then
    echo "==> Downloading linuxdeploy ..."
    curl -fL "$LD_URL" -o "$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
fi
if [ ! -x "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    echo "==> Downloading linuxdeploy-plugin-qt ..."
    curl -fL "$LD_QT_URL" -o "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
fi
chmod +x \
    "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

# Provide the tools under their canonical names on PATH.
ln -sf "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" "$TOOLS_DIR/linuxdeploy"
ln -sf "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" "$TOOLS_DIR/linuxdeploy-plugin-qt"
export PATH="$TOOLS_DIR:$PATH"

# --- 4. Run linuxdeploy ---------------------------------------------------

export APPIMAGE_EXTRACT_AND_RUN=1
# The strip bundled inside linuxdeploy cannot parse modern ELF `.relr.dyn`
# sections, and linuxdeploy treats a failed strip as fatal. Skipping stripping
# only affects binary size, so disable it to stay portable across distros.
export NO_STRIP=1

# Additionally bundle platform plugins beyond the default libqxcb.so so the
# AppImage also runs on Wayland and in headless (offscreen) environments. The
# wayland plugin filename differs between Qt builds, so probe for each
# candidate rather than hard-coding names.
QT_PLUGINS_DIR="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
QT_PLATFORM_DIR="$QT_PLUGINS_DIR/platforms"
EXTRA_PLUGINS=""
for candidate in libqwayland-egl.so libqwayland-generic.so libqwayland.so libqoffscreen.so; do
    if [ -f "$QT_PLATFORM_DIR/$candidate" ]; then
        if [ -n "$EXTRA_PLUGINS" ]; then
            EXTRA_PLUGINS="$EXTRA_PLUGINS;$candidate"
        else
            EXTRA_PLUGINS="$candidate"
        fi
    fi
done
if [ -n "$EXTRA_PLUGINS" ]; then
    export EXTRA_PLATFORM_PLUGINS="$EXTRA_PLUGINS"
    echo "==> Extra platform plugins: $EXTRA_PLATFORM_PLUGINS"
else
    echo "==> No extra platform plugins found in $QT_PLATFORM_DIR (Wayland/offscreen not bundled)."
fi

export VERSION

echo "==> Running linuxdeploy ..."
cd "$WORK"
rm -f "$WORK"/OBS_Desk-*.AppImage
linuxdeploy \
    --appdir "$APPDIR" \
    --executable "$APPDIR_BIN" \
    --desktop-file "$APPDIR_DESKTOP" \
    --icon-file "$APPDIR_ICON" \
    --plugin qt \
    --output appimage

# --- 5. Collect and rename the produced AppImage --------------------------

SRC="$(find "$WORK" -maxdepth 1 -name '*.AppImage' -printf '%T@ %p\n' \
    | sort -n | tail -n1 | cut -d' ' -f2-)"
if [ -z "$SRC" ] || [ ! -e "$SRC" ]; then
    echo "ERROR: linuxdeploy did not produce an AppImage in $WORK." >&2
    exit 1
fi

FINAL="$WORK/OBS_Desk-${VERSION}-x86_64.AppImage"
if [ "$SRC" != "$FINAL" ]; then
    mv -f "$SRC" "$FINAL"
fi
chmod +x "$FINAL"

# --- 6. Report ------------------------------------------------------------

SIZE="$(du -h "$FINAL" | cut -f1)"
echo "==> Done."
echo "==> AppImage: $FINAL"
echo "==> Size    : $SIZE"
