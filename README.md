# EQHeatmap (JUCE VST3) — README

A stereo **EQ heat-map visualizer** plugin built with **JUCE** (VST3 for Windows).
Grid = **10 log-spaced frequency bands** (bottom→top) × **8 pan sectors** (left→right).
Cell color = **green → yellow → red** by averaged level in that frequency/pan cell.

---

## Prerequisites (Windows)

* **CMake** (4.x) in PATH — `cmake --version`
* **Visual Studio 2022** (or Build Tools) with **Desktop development with C++**
* **Git** (optional, for FetchContent to pull JUCE; otherwise you can point to a local JUCE source checkout)

> This project uses JUCE via **CMake FetchContent** (no separate install needed).

---

## One-time clone

```powershell
git clone https://github.com/you/EQHeatmap.git
cd EQHeatmap
```

*(If this is already your working folder, skip.)*

---

## Configure & Build (Release, VST3)

```powershell
# Configure (VS 2022 solution; x64)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build the VST3 target (Release)
cmake --build build --config Release --target EQHeatmap_VST3
```

**Where is the plugin built?**

```
build\EQHeatmap_artefacts\Release\VST3\EQHeatmap.vst3
```

> `.vst3` is a **folder (bundle)** containing the plugin binary.

---

## Install the plugin (no admin)

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

1. Preferences → **Plug-Ins** → enable **Use VST3 Plug-In System Folders**.
2. **Rescan** (hold **Alt** while clicking Rescan to force).
3. Find **EQHeatmap** under **Plug-Ins → VST3** and drop it on a track or the **Master**.

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


## How to use `rebuild-and-install.ps1`
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
