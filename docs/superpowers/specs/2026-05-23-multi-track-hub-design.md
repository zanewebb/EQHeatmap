# Multi-Track Hub + Satellites — Roadmap Spec

**Date:** 2026-05-23
**Status:** Future / not scheduled. Captured as a roadmap reference.

## Goal

Show every track's spectrum on a single overlaid heatmap with per-track colors, so a mix engineer can see "where each element lives in the stereo soundstage" without having to A/B between solo'd tracks. Hovering a track name dims all but that track's contribution.

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
