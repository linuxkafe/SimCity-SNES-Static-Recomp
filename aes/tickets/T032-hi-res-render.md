---
ticket: T032
title: Optional 2× internal resolution (hi-res pixmaps)
type: feature
phase: plan
status: open
priority: low
closure_condition: "Config flag `hi_res=1` renders core at 796×478 (2×) into an intermediate buffer, then down-scales with bilinear filter; visual quality improves, frame time increase documented (<2×)."
---

## Context
User asked whether pixmap resolution can be increased. The core currently outputs 256×239 (standard) or 398×239 (widescreen). A 2× internal render (512×478 or 796×478) with a high-quality down-scale would give crisper text and tile edges on modern displays.

## Acceptance criteria
1. Add `hi_res` boolean to `SimCityLinuxConfig` (settings.ini `[General] HiRes=0|1`).
2. When enabled, `simcity_recomp_render_current_frame` allocates a 2× BGRA buffer, calls the core with `output_width = 2 * base_width`, then uses SDL_RenderCopy with `SDL_RenderSetScale(renderer, 0.5, 0.5)` or a custom bilinear downscale shader.
3. Document frame-time impact (measure with `headless-profile --window 300:900`).
4. Ensure widescreen mode works correctly at 2× (796×478).
5. All tests pass; no regression when `HiRes=0`.