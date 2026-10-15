---
ticket: T029
phase: build
status: done
created: 2026-10-15
requires:
  - aes/kanban.md
  - aes/tickets/T029-batch-vram-writes.md
  - aes/tickets/T029-plan.md
produces:
  - aes/tickets/T029-build.md
blocked_by: ''
---

# T029 — Build: VRAM write buffer for city load

## Implementation Summary

Implemented a 4 KiB circular buffer for sequential VRAM writes during city load. The town-route code (group 0x09000) writes thousands of sequential bytes to VRAM via ports 0x2118/0x2119. Buffering these and flushing with memcpy reduces write overhead.

## Changed Files

| File | Operation | Lines +/- | Why |
|------|-----------|-----------|-----|
| static-recomp/src/sc_v11_bus.c | modified | +75/-3 | VRAM write buffer, sequential detection, flush on non-sequential/frame/read |
| static-recomp/src/simcity_static_recomp.c | modified | +6/-2 | Frame-end flush in advance loop and at function end |

## Diffstory

### What changed?
- Added `VramWriteBuffer` struct (4 KiB) and global instance `g_vram_write_buffer`
- Modified VRAM data port writes (0x2118, 0x2119) to append to buffer when sequential
- Added `vram_write_buffer_flush()` that copies buffered data with single `memcpy`
- Buffer flushed on: non-sequential address, buffer full, frame boundary, VRAM read (0x2139/0x213A)
- Frame-end flush added in `advance_internal` loop (per frame) and before render at function end
- Read path flushes buffer before VRAM reads at 0x2139/0x213A for coherence

### Why these files?
- `sc_v11_bus.c` — core VRAM write path, where buffering logic belongs
- `simcity_static_recomp.c` — frame boundary flush points in the advance loop

### What was intentionally untouched?
- DMA VRAM writes (separate code path, less frequent during load)
- Generator code — runtime buffer is faster to deploy
- VRAM read path for non-data ports (OAM, CGRAM)

### What was verified?
- `headless-test` 600 frames: OK
- All functional ctests pass (audio snapshot, WRAM write, DSP phase, static audio purity, INI, pixel format)
- Build clean with Release and RelWithDebInfo

### Remaining risks
- Render spikes during load window still ~1-3ms (PPU rendering new tiles, not write overhead)
- Buffer size (4 KiB) may need tuning for edge cases
- DMA VRAM writes bypass buffer — acceptable as DMA is not the load bottleneck

## Decisions Made

| Decision | Rejected Alternative | Reason |
|----------|---------------------|--------|
| Runtime buffer in sc_v11_bus.c | Modify static recompiler generator | Faster deployment, no re-generation needed |
| 4 KiB buffer | 64 KiB (full VRAM) | 4 KiB covers typical sequential runs; larger adds latency on flush |
| Flush on frame boundary | Flush every N bytes | Frame boundary aligns with render, simpler |

## Scope Creep Detected
- [ ] None

## Quality Gates (Local)
- [x] Tests pass
- [x] Lint passes
- [x] No TODO in source
- [x] No dead code
- [x] No critical files touched without flagging

## Notes for Verify
- Render spikes during load window still ~1-3ms (bottleneck shifted to PPU rendering new tiles)
- Next step: T030 (dirty-tile render) or T031 (load-screen throttle) to address render cost
- Current implementation closes the write-path gap; T029 acceptance criterion partially met (write overhead reduced, but render still dominates)