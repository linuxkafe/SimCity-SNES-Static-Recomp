---
ticket: T030
title: Dirty-tile render path
type: feature
phase: plan
status: open
priority: high
closure_condition: "PPU only recomputes 8×8 tiles that changed since last frame; render time on scrolling stays ≤0.2 ms/frame and load-window render spikes ≤1 ms/frame."
---

## Context
Current scroll-cache shifts whole framebuffer. A dirty-tile approach would track which VRAM tiles (8×8) were written and only re-render those, eliminating full VRAM walk even when camera doesn't move but tile data changes.

## Acceptance criteria
1. Add a 256×256-bit dirty-tile bitmap (one bit per 8×8 tile) in `SCMachine` or `SimCityRecomp`.
2. Hook VRAM write paths (`sc_machine_vram_write8/16`, DMA) to set bits.
3. Modify `sc_v28_render_first_visible_frame` to iterate only dirty tiles when a full render is required.
4. Verify on load window: render spikes ≤1 ms/frame; on scrolling: ≤0.2 ms/frame.
5. All existing tests pass.