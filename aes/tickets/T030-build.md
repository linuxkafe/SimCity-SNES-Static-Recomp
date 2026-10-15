---
ticket: T030
phase: build
status: done
created: 2026-10-15
requires:
  - aes/kanban.md
  - aes/tickets/T030-dirty-tile-render.md
  - aes/tickets/T030-plan.md
produces:
  - aes/tickets/T030-build.md
blocked_by: ''
---

# T030 — Build: Dirty-tile bitmap and marking infrastructure

## Implementation Summary

Implemented the dirty-tile bitmap infrastructure for partial PPU render. Added a 16 KiB dirty-tile bitmap to `SCV11Runtime`, marking functions in `sc_v11_bus.c`, and integration with VRAM write paths (bus, DMA) and frame-end clearing.

## Changed Files

| File | Operation | Lines +/- | Why |
|------|-----------|-----------|-----|
| static-recomp/internal/include/sc_v11_runtime.h | modified | +4/-1 | Add `vram_dirty_tiles[16384]` bitmap and function declarations |
| static-recomp/src/sc_v11_bus.c | modified | +35/-6 | Mark dirty on VRAM writes (0x2118/0x2119), DMA path; clear at frame end |

## Diffstory

### What changed?
- Added `uint8_t vram_dirty_tiles[16384]` to `SCV11Runtime` (16 KiB, covers 128×128 logical tiles)
- Added `sc_v11_vram_mark_tile_dirty`, `sc_v11_vram_mark_range_dirty`, `sc_v11_vram_clear_dirty` in `sc_v11_bus.c`
- Modified VRAM data port writes (0x2118, 0x2119) to mark corresponding tiles dirty
- Added frame-end dirty bitmap clear in `advance_internal`
- Added declarations in `sc_v11_runtime.h`

### Why these files?
- `sc_v11_runtime.h` — Core runtime struct, central place for dirty bitmap
- `sc_v11_bus.c` — All VRAM write paths (bus, DMA, PPU registers) converge here

### What was intentionally untouched?
- `sc_v28_video.c` renderer — Partial render using dirty bitmap is next phase
- DMA dirty marking — DMA writes to 0x2118/0x2119 go through `sc_v11_ppu_write_raw` which already marks dirty
- CHR (tile graphics) dirty tracking — Tilemap is the load bottleneck

### What was verified?
- `headless-test` 600 frames: OK
- All functional ctests pass (audio snapshot, WRAM write, DSP phase, static audio purity, INI, pixel format)
- Build clean with Release and RelWithDebInfo

### Remaining risks
- Partial render in `sc_v28_video.c` not yet implemented — this is the next phase
- Dirty bitmap size (16 KiB) covers 128×128 logical tiles; actual BG tilemaps are 32×32 or 64×64 — mapping is approximate
- DMA dirty marking relies on `sc_v11_ppu_write_raw` being called for 0x2118/0x2119 during DMA (verified)
- Tile index mapping from VRAM address to dirty bitmap index is approximate (linear VRAM addr → 128×128 logical tilemap)

## Decisions Made

| Decision | Rejected Alternative | Reason |
|----------|---------------------|--------|
| 16 KiB bitmap (128×128 tiles) | 64 KiB (256×256) | 16 KiB covers typical BG tilemaps (32×32, 64×64) with room |
| Mark dirty in bus.c | Mark dirty in generator | Runtime marking works for all write paths (bus, DMA, PPU) |
| Clear at frame end in advance_internal | Clear in render function | Advance_internal is the frame boundary for both headless and frontend |

## Scope Creep Detected
- [ ] None

## Quality Gates (Local)
- [x] Tests pass
- [x] Lint passes
- [x] No TODO in source
- [x] No dead code
- [x] No critical files touched without flagging

## Notes for Verify
- Partial render in `sc_v28_video.c` is the next phase (separate build)
- Current implementation provides the marking infrastructure; render optimization is separate
- Current performance: load window render spikes still ~1-3ms (partial render pending)
- Tile index mapping from linear VRAM address to 128×128 bitmap is approximate; may over-mark