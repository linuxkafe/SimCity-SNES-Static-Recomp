---
ticket: T031
phase: plan
status: done
created: 2026-10-15
tier: standard
requires:
  - aes/kanban.md
  - aes/tickets/T031-load-screen-throttle.md
produces:
  - aes/tickets/T031-plan.md
blocked_by: ''
---

# T031 — Plan: Load-screen render throttle

## Reconnaissance Summary

**Problem:** During city load (frames 300-900), the PPU render takes 1-5ms/frame even though the screen shows a static "loading" screen. The ROM sets a WRAM flag indicating loading state. We can skip full PPU render during this period.

**Current state:** T029 (VRAM write buffer) and T030 infra (dirty tile bitmap) are done. Render spikes during load window are 1-3ms. The frame cache handles static screens, but loading screen may have minor changes (animation).

**Target:** Detect loading state via WRAM flag, skip full PPU render for N frames (configurable, default 4), reusing previous framebuffer.

**Key files:**
- `static-recomp/src/simcity_static_recomp.c` — render path, frame-end clear
- `static-recomp/internal/include/simcity_static_recomp.h` — config struct for throttle count
- `frontend/linux/settings_ini.h/c` — config persistence

## Two-Agent Analysis (standard tier)

### Critic Phase

**Como se fosse uma criança:** "Quando o jogo tá carregando, a tela não muda quase nada. Por que redesenhar tudo?"

**Como se fosse um especialista:** During city load, the ROM displays a static loading screen. The PPU still walks all VRAM/tilemap every frame (1-3ms). A WRAM flag (to be identified) indicates loading state. Skipping render for N frames while reusing the last valid framebuffer eliminates this cost.

**FAILURE MODE 1 — Wrong WRAM flag**
Mechanism: Using the wrong address/value causes skipping render during gameplay or not skipping during load.

**FAILURE MODE 2 — Visual glitches**
Mechanism: If the loading screen has subtle animation (e.g., spinner), skipping frames loses it.

**FAILURE MODE 3 — State desync**
Mechanism: Frame cache hash doesn't account for skipped renders; subsequent frames may have stale cache.

**ASSUMPTION**
A specific WRAM address holds a "loading" flag during city generation. If false: need to find correct flag or use heuristic (e.g., frame count after "New City" confirmation).

**ALTERNATIVE FRAMING**
Instead of WRAM flag, use frame count heuristic: skip render for 60 frames after "New City" confirmed.

**Porquê? ×5**
1. Why WRAM flag? → Most reliable, game-authoritative
2. Why not frame count? → Brittle, varies by ROM version
3. Why skip render? → 1-3ms savings per frame during load
4. Why configurable N? → Balance visual fidelity vs performance
5. Why now? → T029/T030 infra done; this is the final load optimization

### Implementor Phase

**Como se fosse uma criança:** "A gente vê se o jogo tá carregando. Se tá, não desenha de novo, usa o desenho antigo."

**Como se fosse um especialista:** Find the WRAM loading flag by tracing writes during load window. Add `loading_skip_frames` counter to `SimCityRecomp`. In `render_current_frame`, if counter > 0, decrement and return early (copy previous framebuffer). Reset counter when loading flag clears.

**ADDRESSING FAILURE MODE 1**
Trace WRAM writes during load window (frames 300-900) to find loading flag. Common addresses: 0x0000-0x1FFF (system), 0x7E0000+ (WRAM).

**ADDRESSING FAILURE MODE 2**
Default skip count = 4 frames (covers ~66ms at 60fps). Loading animation typically updates every 8-16 frames.

**ADDRESSING FAILURE MODE 3**
Frame cache hash includes framebuffer; skipped frames reuse previous framebuffer so hash matches.

**ADDRESSING ALTERNATIVE FRAMING**
Start with WRAM flag approach; fallback to frame-count heuristic if flag not found.

## Hostile Analysis

**ASSUMPTIONS I AM MAKING:**
- [INFERRED] ROM sets a WRAM flag during city load — need to identify via tracing
- [ASSUMED] Loading screen is static enough that skipping 4 frames is imperceptible
- [UNKNOWN] Exact WRAM address/value for loading flag

**WHAT WAS NOT SPECIFIED:**
- Exact WRAM address for loading flag

**ALTERNATIVES NOT CHOSEN:**
- Frame-count heuristic — less reliable across ROM versions
- Full render every N frames — simpler but less savings

**RISKS AND SIDE EFFECTS:**
- Visual regression if loading screen has animation
- Frame cache inconsistency if not handled correctly

**COST OF BEING WRONG:** medium — visual glitch during load only

**SCOPE BOUNDARY:**
In scope: WRAM flag detection, skip counter, framebuffer reuse, config option
Out of scope: Automatic flag discovery (manual tracing required), loading animation preservation

## Technical Approach

**Chosen approach:** WRAM flag detection + render skip counter

1. **Identify loading flag:** Use `headless-profile --watch` during load window to find WRAM address that changes during load
2. **Add config:** `loading_skip_frames` (default 4) to `SimCityLinuxConfig`
3. **Add runtime state:** `uint8_t loading_skip_remaining` in `SimCityRecomp`
4. **Render path:** In `render_current_frame`, check loading flag → if set and counter > 0, copy previous framebuffer, decrement counter, return
5. **Config:** Expose via `settings.ini` as `LoadingSkipFrames`

## Affected Files

| File | Operation | Description |
|------|-----------|-------------|
| static-recomp/src/simcity_static_recomp.c | modify | Add loading skip logic in `render_current_frame` |
| static-recomp/src/simcity_static_recomp.c | modify | Add `loading_skip_remaining` to `SimCityRecomp` struct |
| frontend/linux/settings_ini.h | modify | Add `loading_skip_frames` to config struct |
| frontend/linux/settings_ini.c | modify | Load/save `LoadingSkipFrames` |
| frontend/linux/menu.c | modify | Add menu row for loading skip frames |

## Specification

**New struct field:**
```c
uint8_t loading_skip_remaining;  // frames remaining to skip render
```

**Config default:** `loading_skip_frames = 4`

**Loading flag detection:** TBD via tracing (candidate: WRAM 0x013D or similar)

## Testing Strategy

1. **Trace:** Run load window with `--watch` on candidate WRAM addresses
2. **Unit:** Skip counter decrements correctly, framebuffer copied correctly
3. **Integration:** Load window profile — average render ≤ 1ms/frame
4. **Visual:** Verify no glitches during load (manual)
5. **Regression:** All ctest tests pass

## Verification Criteria

- [ ] Load window (frames 300-900) average render ≤ 1 ms/frame
- [ ] `headless-test` 600 frames passes
- [ ] All ctest tests pass
- [ ] No visual artifacts during load (manual check)
- [ ] Config persists in settings.ini

## Estimation

- Complexity: medium (4-6 hours)
- Risk: medium
- Blocking dependencies: need to identify WRAM loading flag

---

**Plan complete for T031.**
Output: `aes/tickets/T031-plan.md`
Next step: `/aes-build` (implement load-screen throttle)