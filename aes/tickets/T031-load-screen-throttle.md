---
ticket: T031
title: Load-screen render throttle
type: feature
phase: plan
status: open
priority: medium
closure_condition: "When a WRAM 'loading' flag is detected, PPU render is skipped for N consecutive frames (configurable, default 4), reusing previous framebuffer; load-window average render ≤1 ms/frame."
---

## Context
During city generation the ROM sets a WRAM flag (to be identified) indicating a loading screen. The PPU still walks all VRAM each frame, causing 0.8‑5 ms spikes. A simple throttle that detects the flag and skips full render for a few frames eliminates those spikes.

## Acceptance criteria
1. Identify the WRAM address/value the ROM uses for "loading" state (search in `study/` or trace writes).
2. Add `uint8_t loading_skip_frames` to `SimCityRecomp`; when flag is set, decrement counter and return early from `render_current_frame` (copy previous framebuffer).
3. Make skip count configurable via `settings.ini` (default 4).
4. Profile load window (`--window 300:900`) – average render ≤1 ms/frame.
5. All tests pass.