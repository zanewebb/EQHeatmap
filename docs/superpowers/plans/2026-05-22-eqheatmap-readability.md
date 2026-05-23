# EQHeatmap Readability Pass — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the heatmap "read at a glance" by collapsing low-energy cells toward black, glowing peaks toward white, and adding a precise cursor readout (frequency / pan / approx-dB) with crosshair guides.

**Architecture:** Three small, independent changes — (1) a display-curve + palette-tail + raised-gate cleanup pass in the renderer, (2) a hover HUD with crosshair and a "Follow Cursor" toggle, (3) bumping the Gamma default. No new files in `Source/`; all changes confined to `Theme.{h,cpp}`, `PluginEditor.{h,cpp}`, `PluginProcessor.cpp`.

**Tech Stack:** JUCE 8.0.9 (C++17), CMake + Ninja. Targets VST3 + AU on macOS. Build via `./rebuild-and-install.sh`. Smoke-test in JUCE's AudioPluginHost (already built at `/tmp/aph-wrap/build/juce-build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.app`) with BlackHole-routed audio.

**No automated tests:** the existing project has no test target and the changes are visual + mouse-interaction. Verification is "build, install, open in APH, look at it." That's the right mechanism for this codebase; this plan does not invent test infrastructure that doesn't exist.

**Spec:** [`docs/superpowers/specs/2026-05-22-eqheatmap-readability-design.md`](../specs/2026-05-22-eqheatmap-readability-design.md).

---

## Section 1: Signal/Noise Cleanup

### Task 1: Add `applyDisplayCurve` helper to Theme

**Files:**
- Modify: `Source/Theme.h` (add declaration in `eq` namespace, near `magma`)
- Modify: `Source/Theme.cpp` (add definition near the top of the file)

- [ ] **Step 1: Declare the helper in `Theme.h`**

In `Source/Theme.h`, immediately after the existing `juce::Colour magma (float t) noexcept;` declaration, add:

```cpp
// Maps [0,1] -> [0,1] but compresses low values toward 0 and expands high values.
// Use before magma() so low-energy cells collapse to black and peaks dominate the color range.
// applyDisplayCurve(0.0)  == 0.0
// applyDisplayCurve(0.05) ≈ 0.16
// applyDisplayCurve(0.5)  ≈ 0.76
// applyDisplayCurve(1.0)  == 1.0
inline float applyDisplayCurve (float v) noexcept
{
    return std::log1p (9.0f * juce::jlimit (0.0f, 1.0f, v)) / std::log (10.0f);
}
```

Also ensure `<cmath>` is included at the top of `Theme.h` (it isn't currently — add `#include <cmath>` alongside the existing `#include <JuceHeader.h>`).

- [ ] **Step 2: Commit**

```bash
git add Source/Theme.h
git commit -m "Add applyDisplayCurve helper for log compression of cell values"
```

---

### Task 2: Extend `magma()` palette with hot-white tail

**Files:**
- Modify: `Source/Theme.cpp` (function `magma`, the `stops[][3]` array)

- [ ] **Step 1: Add a 12th stop at pure white**

In `Source/Theme.cpp`, find the `stops[][3]` static array inside `magma()`. The current last line is:

```cpp
    { 0.987f, 0.991f, 0.749f },
};
```

Add a new line directly before the closing `};`:

```cpp
    { 0.987f, 0.991f, 0.749f },
    { 1.000f, 1.000f, 1.000f },
};
```

No other code in `magma()` needs to change — the function already computes `n` from `sizeof(stops)`.

- [ ] **Step 2: Commit**

```bash
git add Source/Theme.cpp
git commit -m "Extend magma palette with hot-white tail so peak cells glow"
```

---

### Task 3: Apply display curve + raise noise gate in `paint()`

**Files:**
- Modify: `Source/PluginEditor.cpp` (the per-pixel loop inside `paint()` that fills `heatmapImage`)

- [ ] **Step 1: Wire `applyDisplayCurve` through and raise the gate threshold**

In `Source/PluginEditor.cpp`, find the block in `paint()` that uploads cell values into `heatmapImage`. It currently looks like:

```cpp
    {
        juce::Image::BitmapData bd (heatmapImage, juce::Image::BitmapData::writeOnly);
        for (int fy = 0; fy < rows; ++fy)
        {
            const int yDst = rows - 1 - fy;
            for (int px = 0; px < cols; ++px)
            {
                const float v = processor.getCellValue (fy, px);
                const juce::Colour c = (v <= 0.001f) ? juce::Colour (juce::uint32 (0))
                                                     : eq::magma (v);
                bd.setPixelColour (px, yDst, c);
            }
        }
    }
```

Replace it with:

```cpp
    {
        juce::Image::BitmapData bd (heatmapImage, juce::Image::BitmapData::writeOnly);
        for (int fy = 0; fy < rows; ++fy)
        {
            const int yDst = rows - 1 - fy;
            for (int px = 0; px < cols; ++px)
            {
                const float v = processor.getCellValue (fy, px);
                if (v <= 0.05f) {
                    bd.setPixelColour (px, yDst, juce::Colour (juce::uint32 (0)));
                    continue;
                }
                bd.setPixelColour (px, yDst, eq::magma (eq::applyDisplayCurve (v)));
            }
        }
    }
```

Two changes: gate threshold went from `0.001f` to `0.05f`, and `magma(v)` became `magma(applyDisplayCurve(v))`.

- [ ] **Step 2: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "Apply display curve + raise noise gate in heatmap renderer"
```

---

### Task 4: Build and visually verify Component 1

- [ ] **Step 1: Quit any open AudioPluginHost instance**

```bash
osascript -e 'tell application "AudioPluginHost" to quit' 2>/dev/null || true
```

(Required because macOS will refuse to overwrite an in-use plugin bundle.)

- [ ] **Step 2: Build and install**

```bash
./rebuild-and-install.sh --kill-daws
```

Expected: ends with `AU VALIDATION SUCCEEDED` and `=== Done ===`. Should take ~30–90 seconds on incremental rebuild (no JUCE re-download).

- [ ] **Step 3: Relaunch AudioPluginHost**

```bash
open /tmp/aph-wrap/build/juce-build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.app
```

- [ ] **Step 4: Verify cleanup visually**

In APH: load the existing graph (Audio Input → EQHeatmap → Audio Output, both pins wired stereo). Set input to BlackHole 2ch. Play music in Spotify/YouTube.

Pass criteria:
- **Silence test:** pause music. The plot should go fully dark within ~1 second (trail decay). Previously had a permanent dim haze.
- **Peak test:** on busy music, the loudest spots should look noticeably brighter than midrange cells — close to white where energy is hottest. No more "everything is the same kind of magenta-pink."
- **Background test:** quiet cells in low-energy frequency regions should be black, not faintly purple.

If any of those fail, debug before continuing — likely culprit: the gate threshold (0.05f) is wrong for your typical input level. Adjust to taste before committing further work.

- [ ] **Step 5: Commit "verified" only if visually OK**

No code change needed; this step is a checkpoint. If you needed to retune any constant, commit that adjustment separately:

```bash
# Only if you tuned anything in Steps 4:
git add Source/PluginEditor.cpp
git commit -m "Tune noise gate threshold after visual smoke test"
```

---

### Task 5: Bump Gamma default

**Files:**
- Modify: `Source/PluginProcessor.cpp` (function `createParameterLayout`, around line 55–56)

- [ ] **Step 1: Change the default**

In `Source/PluginProcessor.cpp`, find:

```cpp
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "gamma", "Gamma",
        juce::NormalisableRange<float> (0.3f, 2.5f, 0.01f), 0.8f));
```

Change `0.8f` (the last argument — the default value) to `1.4f`:

```cpp
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "gamma", "Gamma",
        juce::NormalisableRange<float> (0.3f, 2.5f, 0.01f), 1.4f));
```

- [ ] **Step 2: Build, install, quick visual check**

```bash
./rebuild-and-install.sh --kill-daws
open /tmp/aph-wrap/build/juce-build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.app
```

In APH: remove and re-add the EQHeatmap node (so it picks up the new default), play music, confirm the Gamma slider in the right drawer reads `1.40` and the plot looks tighter (more contrast between bright and dim cells) than with `0.8`.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginProcessor.cpp
git commit -m "Bump Gamma default 0.8 -> 1.4 for tighter contrast"
```

---

## Section 2: Hover HUD

### Task 6: Add `readoutFollowCursor` APVTS param

**Files:**
- Modify: `Source/PluginProcessor.cpp` (function `createParameterLayout`, append at the end of the bleed block)

- [ ] **Step 1: Append the new parameter**

In `Source/PluginProcessor.cpp`, after the last `bleedDecayPct` param, before the function returns, add:

```cpp
    // Readout / HUD
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "readoutFollowCursor", "Readout: Follow Cursor", true));
```

(Place it as the last param push before the function's `return { params.begin(), params.end() };`.)

- [ ] **Step 2: Commit**

```bash
git add Source/PluginProcessor.cpp
git commit -m "Add readoutFollowCursor APVTS param (default true)"
```

---

### Task 7: Add hover state + mouse handling to editor

**Files:**
- Modify: `Source/PluginEditor.h` (add member, declare overrides)
- Modify: `Source/PluginEditor.cpp` (define overrides)

- [ ] **Step 1: Declare hover state in `PluginEditor.h`**

In `Source/PluginEditor.h`, inside the `private:` section of `EQHeatmapAudioProcessorEditor`, after the existing `juce::Rectangle<int> plotArea, controlsPanel;` line, add:

```cpp
    // Hover state for HUD readout. (-1, -1) = cursor not over the plot.
    juce::Point<int> hoverPos { -1, -1 };
```

Also in the class declaration, add these `public:` overrides near `paint`/`resized`:

```cpp
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
```

- [ ] **Step 2: Define the overrides in `PluginEditor.cpp`**

At the bottom of `Source/PluginEditor.cpp` (after `paint()`), add:

```cpp
void EQHeatmapAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    const bool inPlot = plotArea.contains (p);
    const juce::Point<int> next = inPlot ? p : juce::Point<int> { -1, -1 };
    if (next != hoverPos)
    {
        hoverPos = next;
        repaint (plotArea); // cheap: only the plot rect, not the whole editor
    }
}

void EQHeatmapAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoverPos != juce::Point<int> { -1, -1 })
    {
        hoverPos = { -1, -1 };
        repaint (plotArea);
    }
}
```

- [ ] **Step 3: Build to confirm it compiles**

```bash
./rebuild-and-install.sh --kill-daws
```

Expected: clean build (will exit 0). The HUD doesn't render yet but mouse events are wired.

- [ ] **Step 4: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add hover state + mouseMove/mouseExit overrides"
```

---

### Task 8: Render HUD pill in `paint()`

**Files:**
- Modify: `Source/PluginEditor.cpp` (extend `paint()` after the existing axis labels, before the closing brace)

- [ ] **Step 1: Add HUD drawing logic at the end of `paint()`**

In `Source/PluginEditor.cpp`'s `paint()` function, **immediately before the closing `}` of `paint`**, append:

```cpp
    // --- Hover HUD ---
    if (hoverPos.x >= 0 && hoverPos.y >= 0 && plotArea.contains (hoverPos))
    {
        using P = EQHeatmapAudioProcessor;

        // Reverse-map cursor (x, y) -> (panIdx, freqIdx) -> (pan, freq, cellValue).
        auto plotInner = plotArea.withTrimmedLeft (64).withTrimmedBottom (30); // mirrors leftLabelW/bottomH
        const float tX = juce::jlimit (0.0f, 1.0f,
            (float) (hoverPos.x - plotInner.getX()) / (float) plotInner.getWidth());
        const float tY = juce::jlimit (0.0f, 1.0f,
            1.0f - (float) (hoverPos.y - plotInner.getY()) / (float) plotInner.getHeight());

        const int panIdx  = juce::jlimit (0, P::kPanBins  - 1, (int) std::round (tX * (P::kPanBins  - 1)));
        const int freqIdx = juce::jlimit (0, P::kFreqBins - 1, (int) std::round (tY * (P::kFreqBins - 1)));

        const float pan  = -1.0f + 2.0f * tX;
        const float freq = processor.getFreqMinHz()
            * std::exp (tY * std::log (processor.getFreqMaxHz() / processor.getFreqMinHz()));
        const float v    = processor.getCellValue (freqIdx, panIdx);

        // Approximate dB using user-set Lower/Upper bounds.
        const float lowerDb = processor.apvts.getRawParameterValue ("rangeLowerDb")->load();
        const float upperDb = processor.apvts.getRawParameterValue ("rangeUpperDb")->load();
        const float approxDb = lowerDb + v * (upperDb - lowerDb);

        // Format the readout string.
        auto fmtFreq = [] (float f) -> juce::String {
            if (f >= 10000.0f) return juce::String ((int) std::round (f / 1000.0f)) + "k Hz";
            if (f >= 1000.0f)  return juce::String (f / 1000.0f, 1) + "k Hz";
            return juce::String ((int) std::round (f)) + " Hz";
        };
        auto fmtPan = [] (float p) -> juce::String {
            if (std::abs (p) < 0.02f) return juce::String ("C");
            return (p >= 0 ? "+" : "") + juce::String (p, 2);
        };
        const juce::String readout = fmtFreq (freq) + "   |   " + fmtPan (pan)
                                   + "   |   ~ " + juce::String ((int) std::round (approxDb)) + " dB";

        // Crosshair lines from cursor to gutters.
        g.setColour (eq::Brand::text.withAlpha (0.25f));
        g.drawLine ((float) hoverPos.x, (float) hoverPos.y,
                    (float) plotArea.getX(), (float) hoverPos.y, 1.0f);
        g.drawLine ((float) hoverPos.x, (float) hoverPos.y,
                    (float) hoverPos.x, (float) plotArea.getBottom(), 1.0f);

        // Pill geometry.
        const int pillW = 200;
        const int pillH = 22;
        const bool follow = processor.apvts.getRawParameterValue ("readoutFollowCursor")->load() > 0.5f;

        int pillX, pillY;
        if (follow)
        {
            pillX = hoverPos.x + 14;
            pillY = hoverPos.y - 32;
            if (pillX + pillW > plotArea.getRight())  pillX = hoverPos.x - 14 - pillW;
            if (pillY < plotArea.getY() + 4)          pillY = hoverPos.y + 18;
        }
        else
        {
            pillX = plotArea.getRight() - pillW - 10;
            pillY = plotArea.getY() + 10;
        }

        auto pill = juce::Rectangle<float> ((float) pillX, (float) pillY, (float) pillW, (float) pillH);
        g.setColour (juce::Colour (0xE6141620));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (eq::Brand::panelEdge);
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setColour (eq::Brand::text);
        g.setFont (juce::Font (juce::FontOptions ("SF Mono", 11.0f, juce::Font::plain)));
        g.drawText (readout, pill.toNearestInt(), juce::Justification::centred, false);
    }
```

Notes on this block:
- ASCII-only string literals (`|` separator, `~` for "approximate"). The spec called for `·` and `≈` but the existing codebase already documents a known MSVC unicode-in-literal warning (see README Tip #7), so ASCII is the safer choice. Reads as `412 Hz   |   +0.18   |   ~ -22 dB`.
- The dB approximation reads `rangeLowerDb` and `rangeUpperDb` directly from APVTS at paint time. Cheap, no caching needed.
- `repaint(plotArea)` from `mouseMove` will trigger this whole block to redraw each cursor move.

- [ ] **Step 2: Build + smoke test**

```bash
./rebuild-and-install.sh --kill-daws
open /tmp/aph-wrap/build/juce-build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.app
```

In APH: hover over the heatmap. Pass criteria:
- HUD pill appears, follows the cursor with the offset (`+14, -32`).
- Frequency / pan / dB values update in real time.
- Crosshair lines appear from cursor to left gutter and bottom strip.
- Near the right edge of the plot, the pill flips to the left of the cursor.
- Near the top edge, the pill flips below the cursor.
- HUD disappears when the cursor leaves the plot.

- [ ] **Step 3: Commit**

```bash
git add Source/PluginEditor.cpp
git commit -m "Render hover HUD with frequency / pan / approx-dB readout and crosshair"
```

---

### Task 9: Add Follow Cursor toggle UI

**Files:**
- Modify: `Source/PluginEditor.h` (add toggle member + attachment)
- Modify: `Source/PluginEditor.cpp` (instantiate + lay out + bind)

- [ ] **Step 1: Declare the toggle and attachment in `PluginEditor.h`**

In `Source/PluginEditor.h`, inside the `private:` section, find the existing `juce::ToggleButton bleedEnable { "Enable Bleed" };` line. Below the bleed group's controls (after `bleedFreqWidth, bleedPanWidth, bleedDecayPct` and their labels), add:

```cpp
    juce::ToggleButton readoutFollow { "Follow Cursor" };
```

And in the attachment block (after `bleedDecayPctAttachment`):

```cpp
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> readoutFollowAttachment;
```

- [ ] **Step 2: Instantiate and attach in the constructor**

In `Source/PluginEditor.cpp`, in the constructor `EQHeatmapAudioProcessorEditor::EQHeatmapAudioProcessorEditor`, find the existing line:

```cpp
    bleedDecayPctAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "bleedDecayPct",  bleedDecayPct);
```

Immediately after it, add:

```cpp
    addAndMakeVisible (readoutFollow);
    readoutFollowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "readoutFollowCursor", readoutFollow);
```

- [ ] **Step 3: Lay it out in `resized()`**

In `Source/PluginEditor.cpp`'s `resized()` function, find the end of the Bleed group block:

```cpp
    placeRow (bInner, lblBleedFreq,  bleedFreqWidth);
    placeRow (bInner, lblBleedPan,   bleedPanWidth);
    placeRow (bInner, lblBleedDecay, bleedDecayPct);
}
```

Immediately before the closing `}` of `resized()`, add:

```cpp
    col.removeFromTop (14);

    // Single toggle pill, no group box — it's a one-control row.
    auto readoutRow = col.removeFromTop (22);
    readoutFollow.setBounds (readoutRow);
```

- [ ] **Step 4: Build + smoke test**

```bash
./rebuild-and-install.sh --kill-daws
open /tmp/aph-wrap/build/juce-build/extras/AudioPluginHost/AudioPluginHost_artefacts/Release/AudioPluginHost.app
```

In APH: the right drawer should now show a **Follow Cursor** pill at the bottom (below the Bleed group). It starts ON. Hover the heatmap → HUD follows cursor. Click the toggle to OFF → HUD jumps to the top-right corner of the plot and stays there as you move the cursor. Click back ON → returns to cursor-follow.

- [ ] **Step 5: Commit**

```bash
git add Source/PluginEditor.h Source/PluginEditor.cpp
git commit -m "Add Follow Cursor toggle in drawer; wire to readoutFollowCursor param"
```

---

## Section 3: Wrap-up

### Task 10: Full-pass smoke + push

- [ ] **Step 1: Final sanity build**

```bash
./rebuild-and-install.sh --kill-daws
```

Expected: clean build, `AU VALIDATION SUCCEEDED`.

- [ ] **Step 2: End-to-end visual check in APH**

Open AudioPluginHost, play stereo music via BlackHole. Verify the full feature set:

1. **Cleanup (Section 1):**
   - Dark plot during silence
   - Bright peaks during loud passages, with white at the hottest cells
   - No haze in quiet frequency regions

2. **HUD (Section 2):**
   - Hover anywhere on plot → readout shows `<freq> Hz · <pan> · ≈ <db> dB`
   - Crosshair lines from cursor to gutters
   - Edge flips: pill flips left near right edge, flips down near top edge
   - Toggle "Follow Cursor" OFF → pill pins to top-right corner
   - Toggle back ON → pill returns to cursor-follow

3. **Defaults (Task 5):**
   - Gamma slider reads `1.40` on a freshly added plugin instance

4. **Persistence:**
   - In APH, **File → Save Plug-in State As…** for the EQHeatmap node, **File → Load Plug-in State From File…** — Follow Cursor and Gamma settings should round-trip.

- [ ] **Step 3: Push to main**

```bash
git push origin main
```

Expected: all readability-pass commits land on `zanewebb/EQHeatmap` `main`.

- [ ] **Step 4: Stop the brainstorm server**

```bash
/Users/shahzodaakhmedova/.claude/plugins/cache/claude-plugins-official/superpowers/5.1.0/skills/brainstorming/scripts/stop-server.sh \
  /Users/shahzodaakhmedova/Documents/GitHub/EQHeatmap/.superpowers/brainstorm/80645-1779512707
```

(Optional — the server self-exits after 30 minutes idle; this is just a tidy cleanup.)
