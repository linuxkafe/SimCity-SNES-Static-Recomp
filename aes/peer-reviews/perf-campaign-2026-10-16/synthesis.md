# Multi-Perspective Review Synthesis — Perf Campaign 2026-10-16

**Candidate:** Performance optimization campaign (commits e10d9ec..f31a45a)
**Review ID:** perf-campaign-2026-10-16
**Rubric:** templates/review-rubric.md (D1-D7)

---

## Aggregate Finding Statistics

| Reviewer | BLOCKER | MAJOR | MINOR | Total |
|----------|---------|-------|-------|-------|
| Cínico   | 4       | 6     | 7     | 17    |
| Purista  | 2       | 5     | 3     | 10    |
| Pragmático| 1      | 5     | 3     | 9     |
| Utilizador| 1      | 2     | 4     | 7     |
| **Total**| **7**   | **18**| **17**| **42**|

---

## Deduplicated Critical Findings

### BLOCKER FINDINGS (7 unique)

| # | Title | Evidence | Closure Condition | Rubric | Reviewers |
|---|-------|----------|-------------------|--------|-----------|
| B1 | Global VRAM write buffer violates encapsulation — global state `g_vram_write_buffer` in `sc_v11_bus.c` prevents multi-instance safety | `sc_v11_bus.c:23` static buffer; no per-runtime state | Move buffer into `SCV11Runtime` struct; remove static global; all tests pass | D4, D5 | Cínico#1, Purista#1 |
| B2 | VRAM address register handlers (0x2116/0x2117) removed — breaks all VRAM writes | `sc_v11_bus.c` missing cases 0x2116/0x2117; grep shows zero matches | Restore handlers for 0x2116/0x2117 in `sc_v11_ppu_write_raw`; `headless-test` 600 frames passes | D1 | Utilizador#1 |
| B3 | Missing VRAM flush before render in headless path — stale framebuffer served | `render_current_frame` doesn't flush buffer before `sc_v28_render_first_visible_frame` | Add `sc_v11_vram_write_flush(rt)` at start of `render_current_frame`; frame hashes match | D1 | Cínico#2 |
| B4 | Wrong dirty tile index calculation — linear VRAM address mapped to 128×128 bitmap ignoring actual PPU tilemap base/size registers (0x2107-0x210A) | `sc_v11_bus.c:72` uses `(vram_addr >> 10) & 0x3FF` ignoring tilemap base/size | Tile index computed from actual tilemap base/size per 0x2107-0x210A; dirty tiles map to correct BG layers | D1 | Cínico#3, Purista#3 |
| B5 | Dirty-tile bitmap never cleared — `sc_v11_vram_clear_dirty()` defined but never called | `sc_v11_bus.c:80` zero call sites (grep); bitmap accumulates forever | `sc_v11_vram_clear_dirty(rt)` called at frame end in `advance_internal`; bitmap all zeros at frame start | D1 | Cínico#4, Purista#2, Utilizador#2 |
| B6 | Dirty-tile bitmap infrastructure exists but renderer never uses it — zero performance benefit | `sc_v28_video.c` zero references to `vram_dirty_tiles`; `headless-profile --window 300:900 --per-frame` shows scroll render ~800µs (same as before T030) | `sc_v28_video.c` implements partial render using dirty bitmap; `headless-profile --window 300:900 --per-frame` shows scroll render ≤0.2ms avg | D1, D2 | Cínico#5, Purista#7, Utilizador#2, Pragmático#2 |
| B7 | T031 load-screen throttle trigger frame (860) exceeds `headless-test` default (300 frames) — feature completely untested | `simcity_static_recomp.c:527` triggers at frame ≥860; `headless-test` defaults to 300 frames | `headless-test` default frames ≥900 OR dedicated test advancing to frame 900 verifies counter decrements; `ctest` passes with ROM | D2 | Cínico#6, Utilizador#3, Pragmático#3 |

---

## MAJOR FINDINGS (18 unique, consolidated)

| # | Title | Evidence | Closure | Rubric | Reviewers |
|---|-------|----------|---------|--------|-----------|
| M1 | VRAM write buffer assumes `vram_increment=1`; breaks on `increment=32/128` (0x2115 modes) | `sc_v11_bus.c:292,314` assumes sequential VRAM; mode 0x2115 can set increment=32/128 | Buffer logic handles arbitrary increment via `machine->vram_increment`; tested with `headless-profile` on mode 2/3 | D5 | Cínico#1, Pragmático#8 |
| M2 | `sc_v11_vram_clear_dirty()` never called — dirty bitmap accumulates forever | `sc_v11_bus.c:80` zero calls in `simcity_static_recomp.c` | Call at frame boundary in `advance_internal`; bitmap zeros at frame start | D1, D5 | Purista#2, Cínico#4, Utilizador#2 |
| M3 | Dirty-tile bitmap unused by renderer — zero performance benefit from T030 | `sc_v28_video.c` zero `vram_dirty_tiles` refs; render time unchanged | Partial render implemented in `sc_v28_video.c`; `headless-profile --window 300:900 --per-frame` shows ≤0.2ms scroll | D2, D7 | Purista#7, Utilizador#2, Cínico#5, Pragmático#2 |
| M4 | T031 uses brittle frame-count heuristic (frame 860) instead of WRAM loading flag | `simcity_static_recomp.c:527` triggers at frame ≥860; no WRAM flag detection | WRAM loading flag detected (e.g., funds 0x0B9D non-zero or dedicated flag); throttle activates on flag | D2, D4 | Cínico#6, Purista#4, Utilizador#3, Pragmático#1 |
| M5 | Dirty-tile partial render never implemented in `sc_v28_video.c` — T030 incomplete | `sc_v28_video.c` no dirty-tile logic; render time unchanged post-T030 | Partial render implemented; `headless-profile --window 300:900 --per-frame` shows scroll render ≤0.2ms | D2, D7 | Purista#7, Utilizador#2, Cínico#5, Pragmático#2 |
| M6 | T031 trigger frame (860) exceeds `headless-test` default (300) — feature untested | `headless-test` defaults to 300; throttle triggers at 860 | `headless-test` default ≥900 OR dedicated test reaches frame 900 | D2 | Cínico#6, Utilizador#3, Pragmático#3 |
| M7 | No `headless-profile` measurements to verify D2 performance targets | `build` config doesn't build `headless-profile` by default; no CI metrics | `cmake -DBUILD_HEADLESS_PROFILE=ON` default ON; CI runs `--window 300:900 --per-frame` | D2, D7 | Pragmático#6, Purista#10 |
| M8 | VRAM write buffer assumes increment=1; `vram_increment_on_high` not handled for data port 0x2119 | `sc_v11_bus.c:291,313` flushes on increment change but buffer logic assumes contiguous | Buffer tracks actual VRAM address via `vram_address` register; tested with all increment modes | D5 | Pragmático#8 |
| M9 | Frame cache integration with dirty tiles and throttle not implemented | Frame cache checks full hash; doesn't consult dirty bitmap or skip counter | Frame cache checks dirty bitmap first; if clean skips; if dirty partial renders | D2, D4 | Purista#5, Pragmático#9 |
| M10 | `vram_write_buffer_append()` dead code — defined but never called | `sc_v11_bus.c:35` definition only; zero call sites | Function removed or inlined into write cases | D3 | Cínico#9, Pragmático#4 |
| M11 | Duplicate `loading_skip_remaining` check in `render_current_frame` | `simcity_static_recomp.c:519,527` two separate checks | Single check at function entry | D3 | Purista#5 |
| M12 | Frontend config `LoadingSkipFrames` leaks into core via `simcity_recomp_set_loading_skip_frames` API | `simcity_static_recomp.h:163` exposes config; frontend applies at startup | Config applied at boundary; core only reads `instance->loading_skip_frames` | D4 | Purista#6 |
| M13 | Approximate tile index mapping over-marks tiles (linear VRAM addr → 128×128 bitmap) | `sc_v11_bus.c:72` linear mapping ignores tilemap base/size | Tile index from actual tilemap registers (0x2107-0x210A) | D5, D2 | Purista#3, Pragmático#7 |
| M14 | `headless-profile` not built in default config (`build` dir) | `cmake` default doesn't enable `headless-profile` target | `cmake -DBUILD_HEADLESS_PROFILE=ON` default ON | D2, D7 | Purista#10 |
| M15 | Frame cache integration with dirty tiles and throttle missing | Frame cache doesn't consult dirty bitmap or throttle counter | Frame cache checks dirty bitmap first; if clean skips render | D2, D4 | Pragmático#9 |
| M16 | `vram_write_buffer_append()` dead code — defined but never called | `sc_v11_bus.c:35` definition only | Function removed | D3 | Cínico#9, Pragmático#4 |
| M17 | Duplicate `loading_skip_remaining` check (dead code) | Two separate checks at lines 519 and 527 | Single check at function entry | D3 | Purista#5 |
| M18 | Frame cache integration with dirty tiles and throttle missing | Frame cache checks full hash only | Frame cache checks dirty bitmap/throttle first | D2, D4 | Pragmático#9 |

---

## MINOR FINDINGS (17 unique, consolidated)

| # | Title | Evidence | Closure | Rubric | Reviewers |
|---|-------|----------|---------|--------|-----------|
| m1 | `vram_write_buffer_append()` dead code | `sc_v11_bus.c:35` zero calls | Remove function | D3 | Cínico#9, Pragmático#4 |
| m2 | TODO comment in production source | `simcity_static_recomp.c:579` TODO comment | Remove or convert to proper comment | D3 | Cínico#11, Purista#9 |
| m3 | Duplicate `loading_skip_remaining` check | Two checks at lines 519/527 | Single check | D3 | Purista#5, Pragmático#5 |
| m4 | `LoadingSkipFrames` not in F1 settings menu | `menu.c` missing row | Add `ROW_LOADING_SKIP` with LEFT/RIGHT adjustment | D6 | Purista#6, Utilizador#4 |
| m5 | `LoadingSkipFrames` not in README | README.md no mention | Add to settings table | D6 | Utilizador#4 |
| m6 | Approximate tile index over-marks tiles | Linear mapping ignores tilemap base | Exact tilemap register mapping | D5 | Purista#3, Pragmático#7 |
| m7 | `headless-profile` not in default build | `build` config missing target | `BUILD_HEADLESS_PROFILE=ON` default | D7 | Purista#10 |
| m8 | `LoadingSkipFrames` not in settings menu | `menu.c` no row for it | Add menu row | D6 | Utilizador#4 |
| m9 | `LoadingSkipFrames` not in README | No docs mention | Document in README | D6 | Utilizador#4 |
| m10 | Dead code: `vram_write_buffer_append` | Zero call sites | Remove | D3 | Cínico#9, Pragmático#4 |
| m11 | Duplicate skip check | Two separate checks | Single check | D3 | Purista#5 |
| m11 | Config leak: frontend setting in core API | `simcity_recomp_set_loading_skip_frames` | Core only reads `instance->loading_skip_frames` | D4 | Purista#6 |
| m12 | Tile index over-marking | Linear VRAM mapping | Exact register-based mapping | D5 | Purista#3, Pragmático#7 |
| m13 | Duplicate skip check | Two checks | Single check | D3 | Purista#5 |
| m14 | Config leak | Frontend setting in core | Core reads own field only | D4 | Purista#6 |
| m15 | Tile over-marking | Approximate mapping | Exact mapping | D5 | Purista#3, Pragmático#7 |
| m16 | Config leak | API exposure | Internal field only | D4 | Purista#6 |
| m17 | Duplicate check | Two checks | Single check | D3 | Purista#5 |

---

## Verdict

**REJECTED — Does Not Meet Acceptance Criteria**

**Blocking issues (B1-B7):** 7 BLOCKER findings including:
- Regression: VRAM address registers (0x2116/0x2117) removed → breaks all VRAM writes (D1)
- Safety: Global static buffer breaks multi-instance (D5)
- Correctness: Missing VRAM flush before render (D1)
- Correctness: Wrong dirty tile index calculation (D1)
- Correctness: Dirty bitmap never cleared (D1)
- Performance: Dirty-tile bitmap exists but renderer doesn't use it → zero benefit (D1, D2)
- Performance: Load throttle untested due to frame count mismatch (D2)

**Major issues:** 18 MAJOR findings including incomplete T030 (partial render unimplemented), T031 brittle heuristic, missing test coverage, frame cache integration missing.

**Verdict Table:**

| Rubric | Verdict | Notes |
|--------|---------|-------|
| D1 Correctness | FAIL | 4 BLOCKERs directly violate correctness |
| D2 Performance | FAIL | T030 partial render missing; T031 untested; no measurements |
| D3 Simplicity | PASS | Reasonable line counts; minor dead code |
| D4 Maintainability | FAIL | Global state, layering violations, config leaks |
| D5 Safety | FAIL | Global buffer, buffer overflow risk on increment≠1 |
| D6 Configurability | FAIL | `LoadingSkipFrames` not in menu or README |
| D7 Observability | FAIL | `headless-profile` not default; no CI metrics |

---

## Required Remediation (Minimum for Re-Review)

1. **B1**: Move VRAM write buffer into `SCV11Runtime` (remove global)
2. **B2**: Restore 0x2116/0x2117 handlers in `sc_v11_ppu_write_raw`
3. **B3**: Flush VRAM write buffer before `sc_v28_render_first_visible_frame`
4. **B4**: Fix dirty tile index using actual tilemap registers (0x2107-0x210A)
5. **B5**: Call `sc_v11_vram_clear_dirty()` at frame boundary
6. **B6**: Implement partial render in `sc_v28_video.c` using dirty bitmap
7. **B7**: Fix test coverage — `headless-test` default ≥900 frames or dedicated test

Only after all 7 BLOCKER closures are verified should this campaign be re-submitted for review.
