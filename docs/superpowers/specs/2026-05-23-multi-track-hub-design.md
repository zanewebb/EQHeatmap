# Multi-Track Hub + Satellites — Roadmap Spec

**Date:** 2026-05-23
**Status:** Ableton-first MVP scoped (see below). Full spec is the long-term target.

## Goal

Show every track's spectrum on a single overlaid heatmap with per-track colors, so a mix engineer can see "where each element lives in the stereo soundstage" without having to A/B between solo'd tracks. Hovering a track name dims all but that track's contribution.

---

## Ableton-First MVP (next session)

A deliberately scoped-down version aimed at one DAW, one OS, one weekend of work. Strip the spec to its essential value: per-track colored overlay on a Mac running Ableton Live.

### Scope reductions vs full spec

- **macOS only.** No Windows shared-memory path. POSIX `shm_open` / `mmap` only.
- **Ableton only for testing.** The plugin itself is DAW-agnostic (VST3 + AU), so it will work in Logic, Reaper, Bitwig identically — but only Ableton is the test target for the MVP.
- **8-satellite cap** in v1. Keeps the color palette simple (8 Wong-distinguishable hues) and the shared-memory scan bounded.
- **No mute/solo per track in v1.** The hub just shows everything that's connected. Mute/solo can be added in v1.1.
- **No persisted track names.** Names live in the satellite's plugin state — when the user reopens the Ableton project, the names re-load with the plugin instance.
- **Additive blend only.** No max-blend or active-only-blend modes in v1.

### Effort breakdown (~10–14 hours of focused work)

| Piece | Effort | Files |
|---|---|---|
| Split CMake into Satellite + Hub plugin targets | 30 min | `CMakeLists.txt` — two `juce_add_plugin` calls, one preprocessor define (`EQHEATMAP_HUB`) |
| macOS POSIX shared-memory layer (`shm_open` + `mmap` wrapper) | 2–3 hr | new `Source/SharedHeatmap.{h,cpp}` (~120 LOC). UUID generation, region naming `/eqheatmap-<uuid>`, header validation, GC by timestamp. |
| Satellite GUI redesign (strip visualizer, add name field + color picker + frame writer) | 2 hr | new `Source/SatelliteEditor.{h,cpp}` (replaces current `PluginEditor` when `EQHEATMAP_HUB` undefined). Reuses `HeatTheme`. |
| Hub overlay renderer (loop over satellites, blend per-color) | 3–4 hr | reworked `paint()` in a new `HubEditor.cpp`. Reuses 90% of existing render code; replaces magma with per-satellite-tint. |
| Hub track-list panel (right drawer addition) | 2–3 hr | new sub-component. Scrollable list, color swatch + name per row, hover-to-isolate handler that dims non-hovered satellites to ~20%. |
| Smoke test in a real Ableton session (3-5 tracks) | 1 hr | iterate on color choices, blend opacity, panel layout |

### What the user does in Ableton (v1 workflow)

1. **First-time setup per project:**
   - Insert the **Satellite** plugin on each track they want visualized (drums, bass, vocals, etc.).
   - In each Satellite UI: type the track name (one-time per project), pick a color (auto-assigned from the 8-color palette, overridable).
   - Insert the **Hub** plugin on the master bus (or any track for visualization-only).
2. **Daily use:** open the Hub UI. See all 8 (or fewer) tracks overlaid with their colors. Hover a track in the side panel to isolate that color visually.

### v1 limitations to flag in release notes

- Track names don't auto-detect from Ableton — must be manually typed in each satellite's GUI.
- Re-opening an Ableton project: Ableton will restore plugin instances with their saved state (names + colors), but the UUIDs are regenerated on plugin reload. Hub will see them as new satellites and re-discover. Should be transparent to the user.
- Max 8 simultaneous satellites. If you need more, queue them or wait for v1.1.

### Path to running this MVP

After implementation:
1. Build a `Hub` AU/VST3 and a `Satellite` AU/VST3 (two bundles, separate names so DAWs distinguish them).
2. The Satellite is what you put on each track. The Hub is what you put on the master.
3. The existing CI workflow (`.github/workflows/release.yml`) already builds VST3 + AU — just needs to be updated to package both bundles.

### When to start

Spec is ready. Effort is bounded. Recommend tackling this as a focused 1–2 weekend project after the current single-plugin readability work has been used in real mixing sessions and any kinks are fixed.

---

## Full Spec (post-MVP target)

## Architecture

Two plugin builds from the same JUCE source:

### Satellite (insert plugin, per track)

- Identical analysis pipeline to current EQHeatmap (`PluginProcessor.cpp` largely unchanged).
- Writes its 192×128 cell grid + track metadata to shared memory each frame.
- Minimal GUI: a track-name text input, a color swatch picker, mute/solo toggles. No visualization (the hub does that).
- Owns a UUID-named POSIX shared-memory region (`/eqheatmap-<uuid>`).

### Hub (insert plugin, master bus)

- No FFT analysis.
- Scans the OS for shared-memory regions matching the prefix `/eqheatmap-*` at startup and periodically.
- For each detected satellite: reads its grid + metadata, blends it into the overlay.
- Renders an overlay heatmap: each satellite's cells colored with that satellite's tag color, composited additively (or with a chosen blend mode).
- Reuses the existing controls (Gamma, Peak Focus, Noise Gate, Bleed, etc.) — they all apply to the composite.
- Adds a track-list side panel: each row shows track name, color swatch, mute, solo, latency indicator.
- Hover-to-highlight: hovering a row in the panel dims all other tracks' contribution to ~20% so the hovered one is isolated visually.

## Shared Memory Layout

Each satellite's region holds:

```
struct EQHeatmapShared {
    uint32_t version;            // protocol version
    uint64_t lastWriteUsec;      // monotonic timestamp for GC
    char     trackName[64];      // satellite-set label
    uint32_t colorARGB;          // satellite-set color
    int32_t  panBins;            // == kPanBins (validation)
    int32_t  freqBins;           // == kFreqBins
    float    cells[kFreqBins][kPanBins];  // analyzed energy
};
```

Total ~96 KB per satellite. Trivial.

## Hub Behavior

- **Auto-discovery:** on each render tick, list `/eqheatmap-*` regions, mmap any new ones, drop any whose `lastWriteUsec` is older than 2 s (stale satellite — DAW closed it).
- **Render order:** stable by UUID so flicker-free even if discovery order changes.
- **Blend modes:** additive (default — sums of energies), max (winner-takes-pixel), active-only (only solo'd tracks visible).
- **Color palette:** 8 perceptually-distinct base hues (Tol or Wong palettes), cycled when more than 8 satellites are connected. User can override per-track.

## Cross-Platform

POSIX shared memory works on macOS and Linux. Windows needs `CreateFileMapping` — different API but same protocol. Defer to a v2 of this spec if Windows support is needed before the v1 ships on macOS.

## Open Questions

- Should the hub auto-register satellites or require explicit "join" from each one?
- Where does the track name come from? DAWs don't expose host track names to plugins via VST3/AU. Likely manual entry in the satellite's GUI on first insert. Persisted in the satellite's plugin state.
- Should the hub also expose per-track gating (turn off track 3 entirely)? Or is that the satellite's mute toggle's job?
- Latency compensation across satellites: trivial for analysis-only plugins (no audio output offset), but worth confirming.

## Out of Scope (v1)

- Cross-DAW or cross-host sync.
- Windows support (defer to v2).
- More than ~32 satellites per hub.
- Recording / playback of historical states.
- Network discovery (LAN-routed satellites).

## Effort Estimate

Realistic 2–4 weekend project once the existing readability pass and per-frame controls are bedded down. The hard parts:

- Shared-memory protocol design and versioning (1 weekend).
- Hub-side overlay renderer (1 weekend — extends current `paint()`).
- Track-list panel + hover-to-isolate UX (1 weekend).
- Cross-platform shm abstraction (only if Windows is in scope — adds 1 weekend).

Satellite-side changes from current EQHeatmap are minimal: strip the visualizer, add the shared-memory write at the end of `processBlock`, add a tiny GUI for name/color.

## Prerequisites Before Starting

The current EQHeatmap should be stable and reflect production-quality decisions for:

- Noise gating defaults
- Display curve shape
- Peak Focus behavior
- HUD readout content

…because the hub will inherit all of these and changing them later means re-tuning across many satellite instances.
