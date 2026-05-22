#!/usr/bin/env bash
# Rebuild & install EQHeatmap (macOS)
# - Cleans build/, configures with Xcode generator, builds Release
# - Copies AU bundle  -> ~/Library/Audio/Plug-Ins/Components
# - Copies VST3 bundle -> ~/Library/Audio/Plug-Ins/VST3
#
# Usage:
#   ./rebuild-and-install.sh                # Release, both formats
#   ./rebuild-and-install.sh --config Debug
#   ./rebuild-and-install.sh --skip-au
#   ./rebuild-and-install.sh --skip-vst3
#   ./rebuild-and-install.sh --kill-daws    # quit common DAWs first to release file locks

set -euo pipefail

CONFIG="Release"
SKIP_AU=0
SKIP_VST3=0
KILL_DAWS=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --config)     CONFIG="$2"; shift 2;;
    --skip-au)    SKIP_AU=1; shift;;
    --skip-vst3)  SKIP_VST3=1; shift;;
    --kill-daws)  KILL_DAWS=1; shift;;
    -h|--help)
      sed -n '2,15p' "$0"; exit 0;;
    *) echo "Unknown arg: $1" >&2; exit 1;;
  esac
done

cyan()  { printf '\033[36m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[33m%s\033[0m\n' "$*"; }

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build"
USER_VST3="$HOME/Library/Audio/Plug-Ins/VST3"
USER_AU="$HOME/Library/Audio/Plug-Ins/Components"

if (( KILL_DAWS )); then
  cyan "=== Quitting DAWs to release file locks ==="
  for app in "Live" "Ableton Live" "Logic Pro" "GarageBand" "REAPER" "AudioPluginHost"; do
    osascript -e "tell application \"$app\" to quit" >/dev/null 2>&1 || true
  done
  # AU validation host
  killall -9 auvaltool >/dev/null 2>&1 || true
fi

cyan "=== Clean build dir ==="
rm -rf "$BUILD_DIR"

# Pick the best available generator.
# Full Xcode.app gives the nicest dev experience (browseable schemes, debugging),
# but Command Line Tools alone is enough — fall back to Ninja, then Unix Makefiles.
pick_generator() {
  local xcode_path; xcode_path="$(xcode-select -p 2>/dev/null || true)"
  if [[ "$xcode_path" == */Xcode.app/* ]] && command -v xcodebuild >/dev/null 2>&1; then
    echo "Xcode"; return
  fi
  if command -v ninja >/dev/null 2>&1; then
    echo "Ninja"; return
  fi
  echo "Unix Makefiles"
}

GENERATOR="$(pick_generator)"
cyan "=== CMake configure (generator: $GENERATOR) ==="
if [[ "$GENERATOR" == "Xcode" ]]; then
  cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR"
else
  # Single-config generators bake CONFIG in at configure time.
  cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE="$CONFIG"
fi

cyan "=== CMake build ($CONFIG) ==="
cmake --build "$BUILD_DIR" --config "$CONFIG" --target EQHeatmap_All

ARTIFACTS="$BUILD_DIR/EQHeatmap_artefacts/$CONFIG"

install_bundle() {
  local src="$1" dest_dir="$2" label="$3"
  if [[ ! -d "$src" ]]; then
    yellow "Skipping $label: bundle not found at $src"
    return
  fi
  mkdir -p "$dest_dir"
  local name; name="$(basename "$src")"
  rm -rf "$dest_dir/$name"
  cp -R "$src" "$dest_dir/"
  green "Installed $label -> $dest_dir/$name"
}

if (( ! SKIP_VST3 )); then
  cyan "=== Install VST3 ==="
  install_bundle "$ARTIFACTS/VST3/EQHeatmap.vst3" "$USER_VST3" "VST3"
fi

if (( ! SKIP_AU )); then
  cyan "=== Install AU ==="
  install_bundle "$ARTIFACTS/AU/EQHeatmap.component" "$USER_AU" "AU"
  # Optional: validate the AU. Non-fatal if it fails (e.g. on first install before cache refresh).
  if command -v auvaltool >/dev/null 2>&1; then
    cyan "=== auval (component validation, non-fatal) ==="
    auvaltool -v aufx EHmp Zane || yellow "auval reported issues — open the plugin in a host to verify."
  fi
fi

cyan "=== Done ==="
echo "Logic / GarageBand: rescan AU on next launch (Logic: hold Control+Option while opening AU manager to reset)."
echo "Ableton Live:       Preferences -> Plug-Ins -> Rescan (hold Alt for forced rescan)."
echo "Test host without a DAW: build JUCE's AudioPluginHost from the JUCE source pulled into build/_deps/juce-src/extras/AudioPluginHost."
