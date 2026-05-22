# EQHeatmap (JUCE) — README

A stereo **EQ heat-map visualizer** plugin built with **JUCE**.
Builds **VST3** on Windows and macOS, plus **AU** on macOS.
Grid = **192 log-spaced frequency bands** (bottom→top) × **128 pan bins** (left→right).
Cell color = averaged level in that frequency/pan cell.

---

## Prerequisites

### Windows
* **CMake** (4.x) in PATH — `cmake --version`
* **Visual Studio 2022** (or Build Tools) with **Desktop development with C++**
* **Git** (optional, for FetchContent to pull JUCE; otherwise you can point to a local JUCE source checkout)

### macOS
* **Xcode** (full app — best dev experience), or **Xcode Command Line Tools** only — `xcode-select --install`
* **CMake** 3.22+ (`brew install cmake`)
* **Ninja** (optional but recommended if you don't have full Xcode — `brew install ninja`)
* Targets **macOS 11.0+** by default (override with `-DCMAKE_OSX_DEPLOYMENT_TARGET=...`)

`rebuild-and-install.sh` auto-detects the best available generator: full Xcode → Ninja → Unix Makefiles.

> This project uses JUCE via **CMake FetchContent** (no separate install needed).

---

## One-time clone

```powershell
git clone https://github.com/you/EQHeatmap.git
cd EQHeatmap
```

*(If this is already your working folder, skip.)*

---

## Configure & Build

### Windows (Release, VST3)

```powershell
# Configure (VS 2022 solution; x64)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build the VST3 target (Release)
cmake --build build --config Release --target EQHeatmap_VST3
```

Output:
```
build\EQHeatmap_artefacts\Release\VST3\EQHeatmap.vst3
```

### macOS (Release, VST3 + AU)

```bash
# One-shot: configure, build, install both formats to ~/Library/Audio/Plug-Ins
./rebuild-and-install.sh

# Or manually:
cmake -S . -B build -G Xcode
cmake --build build --config Release --target EQHeatmap_All
```

Output:
```
build/EQHeatmap_artefacts/Release/VST3/EQHeatmap.vst3
build/EQHeatmap_artefacts/Release/AU/EQHeatmap.component
```

> `.vst3` and `.component` are **folder bundles** containing the plugin binary.

---

## Install the plugin

### Windows (no admin)

Copy the bundle to the **per-user** VST3 folder (scanned by REAPER & Ableton):

```powershell
$src = Resolve-Path '.\build\EQHeatmap_artefacts\Release\VST3\EQHeatmap.vst3'
$dstRoot = "$env:LOCALAPPDATA\Programs\Common\VST3"
New-Item -ItemType Directory -Force $dstRoot | Out-Null
Copy-Item -Recurse -Force $src $dstRoot
```

System-wide (admin):

```
C:\Program Files\Common Files\VST3
```

### macOS (no admin)

`rebuild-and-install.sh` handles this automatically. Manual install:

```bash
cp -R build/EQHeatmap_artefacts/Release/VST3/EQHeatmap.vst3       ~/Library/Audio/Plug-Ins/VST3/
cp -R build/EQHeatmap_artefacts/Release/AU/EQHeatmap.component    ~/Library/Audio/Plug-Ins/Components/
```

Logic / GarageBand load **AU**; Ableton / REAPER / Bitwig load **VST3** (and AU on macOS where supported).

> This project sets `COPY_PLUGIN_AFTER_BUILD TRUE`. If you get a **permission** error during build, either run the build in an **elevated** shell or set a writable copy dir (see **Tips → Control where JUCE copies the plugin**).

---

## Clean rebuild (when in doubt)

```powershell
# Nuke build tree
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Reconfigure + build Release VST3
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target EQHeatmap_VST3
```

---

## Using in your DAW

### REAPER

1. Place `EQHeatmap.vst3` in `%LOCALAPPDATA%\Programs\Common\VST3` (or the system folder).
2. In REAPER: **Options → Preferences → Plug-ins → VST → Clear cache/re-scan**.
3. Add **EQHeatmap (VST3)** as an FX on a track or the **Master**.
4. Click **UI / Float FX** to pop the window; hit **Play** to see the heat-map update.

### Ableton Live

1. Preferences → **Plug-Ins** → enable **Use VST3 Plug-In System Folders** (and **Use Audio Units** on macOS).
2. **Rescan** (hold **Alt** while clicking Rescan to force).
3. Find **EQHeatmap** under **Plug-Ins → VST3** (or **Audio Units → Zane Inc.** on macOS) and drop it on a track or the **Master**.

### Logic Pro / GarageBand (macOS only)

1. Install the **AU** bundle (see above) — Logic/GarageBand only load AU, not VST3.
2. On next launch, Logic runs AU validation automatically. If the plugin doesn't appear, force a fresh scan by deleting `~/Library/Caches/AudioUnitCache/*` and relaunching, or hold **Control+Option** while opening the AU manager.
3. Insert **EQHeatmap** on any track or the Stereo Out.

### Testing without a DAW (macOS or Windows)

JUCE ships an **AudioPluginHost** in its source tree. After your first build it lives at
`build/_deps/juce-src/extras/AudioPluginHost/`. Open the `.jucer` (or build it via its own CMakeLists),
load `EQHeatmap.vst3`, wire an audio input device to the plugin to the audio output, and you have a
zero-friction preview environment.

---

## What you should see

* **Rows (bottom→top):** frequencies (20 Hz → 20 kHz, log-spaced).
* **Columns (L→R):** stereo panorama from hard left to hard right.
* **Color:** green (quiet) → yellow (moderate) → red (hot).
* Example: heavy centered bass → **lower rows** in the **middle columns** turn **red**.

---

## Project structure & key CMake bits

* JUCE is pulled via:

  ```cmake
  include(FetchContent)
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9)
  FetchContent_MakeAvailable(juce)
  ```
* Plugin target:

  ```cmake
  juce_add_plugin(EQHeatmap
      FORMATS VST3
      COPY_PLUGIN_AFTER_BUILD TRUE
      PRODUCT_NAME "EQHeatmap")
  juce_generate_juce_header(EQHeatmap)
  target_link_libraries(EQHeatmap PRIVATE juce::juce_audio_utils juce::juce_dsp)
  ```
* Important compile definition to avoid VST2/VST3 automation conflict:

  ```cmake
  target_compile_definitions(EQHeatmap PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
  ```

---

## Tips & common fixes

### 1) JUCE missing (`find_package(JUCE)` fails)

This project uses **FetchContent**, so you shouldn’t see this.
If you instead point to a local JUCE, use `-DJUCE_DIR="C:/path/to/JUCE/cmake"`.

### 2) `JuceHeader.h: No such file or directory`

Add (already present here):

```cmake
juce_generate_juce_header(EQHeatmap)
```

Then **clean & reconfigure**.

### 3) VST2/VST3 parameter warning stops build

Add:

```cmake
target_compile_definitions(EQHeatmap PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
```

### 4) Auto-copy permission error

```
file cannot create directory: C:\Program Files\Common Files\VST3
```

* Use a **per-user** copy dir:

  ```powershell
  $dst = "$env:LOCALAPPDATA/Programs/Common/VST3"
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_COPY_DIR="$dst"
  ```
* Or run the build in **Administrator** PowerShell.
* Or set `COPY_PLUGIN_AFTER_BUILD FALSE` and copy manually.

### 5) Plugin not found in DAW

* Confirm the **bundle folder** exists:

  ```powershell
  Get-ChildItem -Recurse .\build -Directory | Where-Object { $_.Name -like '*.vst3' }
  ```
* Copy it to one of the VST3 folders (above) and **Rescan** in the DAW.
* Ensure you built **x64** and are running a 64-bit DAW.

### 6) Blank UI when loaded

Make sure the processor returns the **custom editor** (not Generic):

```cpp
juce::AudioProcessorEditor* EQHeatmapAudioProcessor::createEditor() {
  return new EQHeatmapAudioProcessorEditor(*this);
}
```

### 7) Compile warning about Unicode arrows

Replace `"Pan (L \u2190  \u2192 R)"` with ASCII `"Pan (L <-> R)"`,
or save the source as UTF-8 and add `/utf-8` to compile options.

---

## Developer commands cheat-sheet

```powershell
# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build (Release)
cmake --build build --config Release --target EQHeatmap_VST3

# Clean build
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# Copy to per-user VST3
$src = Resolve-Path '.\build\EQHeatmap_artefacts\Release\VST3\EQHeatmap.vst3'
$dst = "$env:LOCALAPPDATA\Programs\Common\VST3\EQHeatmap.vst3"
New-Item -ItemType Directory -Force "$env:LOCALAPPDATA\Programs\Common\VST3" | Out-Null
Copy-Item -Recurse -Force $src $dst

# Verify installed bundle
Get-ChildItem -Recurse "$env:LOCALAPPDATA\Programs\Common\VST3\EQHeatmap.vst3"
```


## How to use `rebuild-and-install.ps1` (Windows)
```
# First time, you may need to allow this script to run in your session:
# Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# Clean, rebuild, install to per-user VST3 (no admin):
.\rebuild-and-install.ps1

# Also copy to system VST3 (will self-elevate for just the copy step):
.\rebuild-and-install.ps1 -CopyToSystem

# If REAPER/Ableton might be locking the file:
.\rebuild-and-install.ps1 -KillDAWs
```

## How to use `rebuild-and-install.sh` (macOS)
```bash
# Clean, rebuild, install both AU + VST3 to ~/Library/Audio/Plug-Ins:
./rebuild-and-install.sh

# Build Debug instead of Release:
./rebuild-and-install.sh --config Debug

# Skip one format:
./rebuild-and-install.sh --skip-au
./rebuild-and-install.sh --skip-vst3

# Quit running DAWs first (release file locks):
./rebuild-and-install.sh --kill-daws
```

---

## Notes & next steps

* Tweak grid size in `PluginProcessor.h`:

  ```cpp
  static constexpr int kPanBins  = 8;
  static constexpr int kFreqBins = 10;
  ```
* Responsiveness: adjust `emaAlpha` (0.15 = steadier, 0.35 = snappier).
* FFT detail vs CPU: increase `kFftOrder` (e.g., 12 → 4096).
* Add tooltips / mid-side view / solo-mute per cell as future enhancements.
