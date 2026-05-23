# EQHeatmap Readability Pass — Design

**Date:** 2026-05-22
**Status:** Approved (pending user spec review)

## Problem

Post-step-3 the visualizer looks polished but is hard to read in three specific ways:

- **B — Peak doesn't pop.** Many cells are warm at once; the single hottest spot doesn't visually pull the eye.
- **C — Frequency identification is fuzzy.** "That hot region is somewhere around low-mid" but pinpointing 250 Hz vs 400 Hz requires squinting at the gutter ticks.
- **D — Quiet background washes everything out.** Every cell carries some color, so peaks don't separate from the noise floor.

D is the root cause: too much low-energy color paints the whole plot, which makes B (peak doesn't pop) and C (gutter labels fight a busy background) worse.

## Goals

1. Make low-energy cells effectively disappear so high-energy cells naturally stand out.
2. Give the user a precise, on-demand readout of frequency, pan, and approximate level under the cursor.
3. Don't add visual chrome that competes with the heatmap itself.

## Non-Goals

- No peak-detection rings, contour lines, or "find the loudest cell" overlays — relying on cleanup to do that work.
- No alternative colormaps. Magma stays; we only extend its top.
- No transient/attack enhancement. Time-domain behavior is out of scope.
- No additional user-tunable params beyond what's strictly needed for the HUD toggle.

## Approach

Three components, all small, all confined to files already touched in steps 2–3.

### Component 1 — Signal/Noise Cleanup

**1a. Display curve before colormap lookup.** New helper in `Theme.h/.cpp`:

```cpp
inline float applyDisplayCurve(float v) noexcept {
    return std::log1p(9.0f * v) / std::log(10.0f);
}
```

Maps [0,1] → [0,1] but compresses the low end and expands the high end. `0.05 → 0.16`, `0.5 → 0.76`, `1.0 → 1.0`. Called in `PluginEditor::paint()` immediately before `eq::magma(v)`.

**1b. Hot-white tail on magma palette.** Append a 12th stop `(1.0, 1.0, 1.0)` to the `stops[][3]` array in `eq::magma()` so the brightest cells glow incandescent-white rather than cream.

**1c. Raise noise gate.** In the per-pixel loop in `paint()`, change `if (v <= 0.001f) continue;` to `if (v <= 0.05f) continue;` so the bottom 5% of intensity stays the dark plot background.

### Component 2 — Hover HUD

**2a. State.** Add to `EQHeatmapAudioProcessorEditor`:
```cpp
juce::Point<int> hoverPos { -1, -1 }; // sentinel: outside plot
```

**2b. Mouse handling.** Override `mouseMove()` and `mouseExit()` to update `hoverPos` (sentinel on exit). Trigger `repaint()` of the union of old + new HUD rects to keep redraw cheap.

**2c. Readout content (when `hoverPos` is inside `plotArea`).** Three values separated by middle dots:
```
412 Hz · +0.18 · ≈ -22 dB
```
- **Frequency**: inverse of log-Y map: `f = fMin * exp((plotBottom - mouseY) / plotH * log(fMax/fMin))`. Format: no decimals above 1 kHz (`1.5k`, `12k`); whole Hz below (`412 Hz`).
- **Pan**: linear `((mouseX - plotLeft) / plotW) * 2 - 1`, formatted `+0.18` / `-0.42`; show `C` if `|pan| < 0.02`.
- **Level (approximate dB)**: `lower + cellValueAtMouse * (upper - lower)`, prefixed with `≈` so the approximation is honest. `cellValueAtMouse` is computed by mapping cursor (x, y) → (`panIdx`, `freqIdx`) via the inverse of the plot transform:
  - `panIdx = round((mouseX - plotLeft) / plotW * (kPanBins - 1))`
  - `freqIdx = round(log((cursorFreqHz) / fMin) / log(fMax/fMin) * (kFreqBins - 1))`
  - Then read `processor.getCellValue(freqIdx, panIdx)`. Clamp both indices to valid range.

**2d. Crosshair.** Thin 1px lines (`eq::Brand::text` at alpha 0.25) from cursor to left gutter and bottom strip. Apex at cursor; legs extend down and left only (not full crosshair across the plot).

**2e. HUD position — user-tunable.** New APVTS param:
```cpp
"readoutFollowCursor" (bool, default true)
```

HUD pill is approximately **180 × 22 px** at the chosen font (SF Mono 11pt + 6px padding × 2 sides). Treat as a fixed-size rect for edge math.

- **Follow Cursor mode (default)**: HUD offset `(+14, -32)` from cursor. Edge flips:
  - If `cursor.x + 14 + pillWidth > plotRight` → flip horizontally to `(-pillWidth - 14, -32)`.
  - If `cursor.y - 32 < plotTop + 4` → flip vertically to `(+14, +18)`.
  - Both flips combine independently if cursor is in the corner.
- **Pinned to Corner mode**: HUD anchored at `(plotRight - pillWidth - 10, plotTop + 10)`.

**2f. Toggle UI.** Add a new pill-style toggle labeled **"Follow Cursor"** at the bottom of the right-side drawer, below the Bleed group. Uses the existing `HeatTheme::drawToggleButton` styling. Bind via `juce::AudioProcessorValueTreeState::ButtonAttachment`.

### Component 3 — Defaults

In `EQHeatmapAudioProcessor::createParameterLayout`:

- **Gamma**: change default `0.8f → 1.4f` (line ~56). Direction confirmed via `v = std::pow(v, gamma)` in `processBlock`: gamma > 1 darkens the bottom of the range.

**Not changing** (already correct):
- Lower dB default `-60.0f` — already at the proposed value.
- Hot Ref default `80.0f` — already above the proposed value; no benefit to lowering.

## Files Touched

| File | Change |
|---|---|
| `Source/Theme.h` | Add `applyDisplayCurve()` inline declaration. |
| `Source/Theme.cpp` | Add 12th `(1,1,1)` stop to `magma()`'s `stops[][3]`. |
| `Source/PluginEditor.h` | Add `hoverPos`, `readoutFollowAttachment`, `Follow Cursor` toggle member. |
| `Source/PluginEditor.cpp` | Wire toggle, override `mouseMove/Exit`, apply `applyDisplayCurve` before `magma()`, raise noise gate to 0.05, add HUD + crosshair drawing. |
| `Source/PluginProcessor.cpp` | Change Gamma default `0.8 → 1.4`; add `"readoutFollowCursor"` `AudioParameterBool`. |

No new files. Estimated diff: ~120–150 LOC.

## Verification

Smoke test in JUCE's AudioPluginHost (already built in this session at `/tmp/aph-wrap/build/...`) with BlackHole-routed Spotify playback:

1. **D fix**: silence should produce a fully dark plot. Solo bass should illuminate only bottom-center, not paint the whole frame.
2. **B fix**: on a complex mix, the eye should land on the loudest element without scanning.
3. **C fix**: hover the cursor on a hot cell. HUD should show a sensible frequency (e.g., a hi-hat ~8 kHz, a kick ~60 Hz), pan reasonable for the source (centered for a kick, hard for stereo hats).
4. **Toggle**: flip "Follow Cursor" off, confirm HUD jumps to top-right corner and stays there.
5. **Persistence**: save state, reload, confirm toggle position survives.

No automated tests — the existing project has none, and these are visual changes.

## Out of Scope (Possible Follow-Ups)

- Peak markers / contour lines (could be added on top of this if cleanup isn't enough).
- Per-frequency-band labeling (bass / low-mid / mid / etc.).
- Configurable colormap (viridis / inferno / custom).
- Mid/side analysis view.
- A/B preset snapshots for comparing mixes.
