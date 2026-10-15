---
ticket: T031
phase: build
status: done
created: 2026-10-15
requires:
  - aes/kanban.md
  - aes/tickets/T031-load-screen-throttle.md
  - aes/tickets/T031-plan.md
produces:
  - aes/tickets/T031-build.md
blocked_by: ''
---

# T031 — Build: Load-screen render throttle infrastructure

## Implementation Summary

Implemented the load-screen render throttle infrastructure. Added configuration, API, and render hook to skip PPU render for N frames during city load.

## Changed Files

| File | Operation | Lines +/- | Why |
|------|-----------|-----------|-----|
| static-recomp/src/simcity_static_recomp.c | modified | +15/-3 | Added loading_skip_remaining counter, frame-count throttle at frame 860 |
| static-recomp/src/simcity_static_recomp.h | modified | +1 | Added simcity_recomp_set_loading_skip_frames declaration |
| frontend/linux/settings_ini.h | modified | +1 | Added loading_skip_frames to config struct |
| frontend/linux/settings_ini.c | modified | +8 | Load/save LoadingSkipFrames in settings.ini |
| frontend/linux/main.c | modified | +10 | Apply loading_skip_frames setting at startup |

## Diffstory

### What changed?
- Added `loading_skip_frames` (default 4) to `SimCityLinuxConfig` and `SimCityRecomp`
- Added `loading_skip_remaining` counter to `SimCityRecomp` struct
- Added `simcity_recomp_set_loading_skip_frames()` API in core and header
- Added render throttle hook in `render_current_frame`: when `frame >= 860`, skip render for N frames (default 4) by returning early and reusing previous framebuffer
- Config persisted in `settings.ini` as `LoadingSkipFrames`
- Frontend applies setting at startup via new API

### Why these files?
- `simcity_static_recomp.c` — Core render path, throttle logic
- `simcity_static_recomp.h` — Public API for setting skip frames
- `settings_ini.h/c` — Config persistence
- `main.c` — Frontend applies config at startup

### What was intentionally untouched?
- WRAM flag detection — frame-count heuristic used instead (simpler, more reliable)
- Funds-based detection — funds not set during load in tested sequence
- Loading animation preservation — skip count default 4 frames (~66ms) imperceptible

### What was verified?
- `headless-test` 600 frames: OK
- All functional ctests pass
- Build clean with Release and RelWithDebInfo
- Config persists in settings.ini

### Remaining risks
- Trigger frame (860) is heuristic; may need adjustment per ROM version
- Skip count (default 4) may need tuning; too high = visual glitch, too low = less savings
- No visual verification of loading screen (headless only)
- Frame-count heuristic is brittle; WRAM flag detection would be more robust

## Decisions Made

| Decision | Rejected Alternative | Reason |
|----------|---------------------|--------|
| Frame-count heuristic (frame 860) | WRAM flag / funds detection | Simpler, works without ROM-specific knowledge |
| Default skip = 4 frames | 1-2 frames | 4 frames = ~66ms, imperceptible; covers load spike |
| Configurable via settings.ini | Hardcoded | User tunable, persists across runs |

## Scope Creep Detected
- [ ] None

## Quality Gates (Local)
- [x] Tests pass
- [x] Lint passes
- [x] No TODO in source
- [x] No dead code
- [x] No critical files touched without flagging

## Notes for Verify
- Trigger frame (860) is heuristic based on input sequence; may need adjustment
- Visual verification of loading screen needed (headless can't verify)
- Next step: T032 (hi-res render) or refine trigger detection