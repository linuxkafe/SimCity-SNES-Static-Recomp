# AES Peer Review Rubric — SimCity SNES Static Recomp Performance Campaign

**Review ID:** perf-campaign-2026-10-16
**Candidate:** Main branch (commits e10d9ec..f31a45a)
**Scope:** Performance optimization tickets T029, T030, T031, T032 infrastructure

---

## Rubric Dimensions

Each dimension is scored PASS / FAIL / N/A. A finding cites exactly one dimension.

| ID | Dimension | Description | Pass Criteria |
|----|-----------|-------------|---------------|
| D1 | **Correctness** | Implementation matches specification; no regressions; all tests pass | All ctests pass; headless-test frame hashes match baseline; no segfaults |
| D2 | **Performance** | Meets or exceeds target metrics documented in ticket | Load window render ≤1ms avg (T031); Scroll render ≤0.2ms (T030); Title/menu ≤13ms (T029) |
| D3 | **Simplicity** | Minimal code change; no speculative abstractions; no dead code | Lines changed ≤ plan estimate; no TODO in source; no unused functions |
| D4 | **Maintainability** | Clear separation of concerns; documented invariants; config via settings.ini | New APIs have headers; config persisted; no hardcoded magic numbers in logic |
| D5 | **Safety** | No data races; buffer bounds checked; graceful degradation | Buffer bounds enforced; frame cache coherent; no segfaults under stress |
| D6 | **Configurability** | User-facing knobs via settings.ini; defaults sensible | loading_skip_frames, HiRes, etc. exposed; defaults documented in README |
| D7 | **Observability** | Measurement hooks present; profiling tools work | headless-profile --window/--dump-pc/--watch works; PC histogram accurate |

---

## Finding Severity Thresholds

| Severity | Trigger |
|----------|---------|
| **BLOCKER** | D1 FAIL, D5 FAIL, or regression in D1 |
| **MAJOR** | D2 FAIL (misses target by >20%), D4 FAIL, D7 FAIL |
| **MINOR** | D3 FAIL, D6 FAIL, missing documentation, suboptimal defaults |

---

## Evidence Requirements

- **D1**: `ctest --test-dir build` output; `headless-test` frame hashes
- **D2**: `headless-profile --window 300:900 --per-frame` output showing ms/frame
- **D3**: `git diff --stat` lines changed vs plan estimate
- **D4**: Header declarations present; settings.ini keys documented
- **D5**: Buffer bounds checks; no ASAN/UBSAN warnings
- **D6**: settings.ini keys present; README updated
- **D7**: `headless-profile --help` shows all flags; PC histogram non-empty

---

## Review Scope

Commits: `e10d9ec..f31a45a` (inclusive)
Tickets: T029, T030, T031, T032 (planned)
Files: `static-recomp/src/sc_v11_bus.c`, `sc_v28_video.c`, `simcity_static_recomp.c`, `sc_v11_runtime.h`, `simcity_static_recomp.h`, `settings_ini.h/c`, `main.c`, `headless-profile.c`