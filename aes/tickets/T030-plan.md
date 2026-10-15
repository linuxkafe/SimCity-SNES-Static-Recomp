---
ticket: T030
phase: plan
status: done
created: 2026-10-15
tier: heavy
requires:
  - aes/kanban.md
  - aes/tickets/T030-dirty-tile-render.md
produces:
  - aes/tickets/T030-plan.md
blocked_by: ''
---

# T030 — Plan: Dirty-tile render path

## Reconnaissance Summary

**Problem:** After T029, VRAM write overhead is minimal, but render spikes during city load remain 1-3ms/frame because the PPU renderer (`sc_v28_render_first_visible_frame`) walks the entire VRAM/tileset every frame, even when only a small subset of tiles changed.

**Current render path:** `sc_v28_render_first_visible_frame` in `static-recomp/src/sc_v28_video.c` iterates all 224 scanlines × 256/398 pixels, calling `background_pixel`/`city_background_pixel` for each layer. This is ~286k pixel operations per frame.

**Target:** Track dirty 8×8 tiles (VRAM tile indices) and only re-render affected tiles. SimCity uses 8×8 tiles for BG layers; typical city load touches ~100-500 tiles/frame, not the full 1024×1024 tilemap.

**Key files:**
- `static-recomp/src/sc_v28_video.c` — PPU renderer
- `static-recomp/internal/include/sc_v11_runtime.h` — dirty tile bitmap in runtime
- `static-recomp/src/sc_v11_bus.c` — VRAM write hook to mark tiles dirty
- `static-recomp/src/simcity_static_recomp.c` — clear dirty map at frame end

## Two-Agent Analysis (heavy tier)

### Critic Phase

**Como se fosse uma criança:** "O jogo desenha a cidade inteira toda vez, mesmo se só mudou um pedacinho. É como repintar a casa toda porque você trocou uma lâmpada."

**Como se fosse um especialista:** The PPU renderer currently processes all visible scanlines and all BG layers unconditionally. SimCity's BG1/BG2 use 8×8 tiles from VRAM; the tilemap is at fixed VRAM addresses. During city load, only ~5% of tiles change per frame. A dirty-tile bitmap (1024×1024 tilemap = 128 KiB bits = 16 KiB) can track which 8×8 tiles were written. The renderer would then only process scanlines intersecting dirty tiles.

**FAILURE MODE 1 — False negatives (missed dirty tiles)**
Mechanism: VRAM writes via DMA, HDMA, or PPU register side-effects bypass the bus write hook, leaving tiles unmarked dirty.

**FAILURE MODE 2 — False positives (over-marking)**
Mechanism: Writing to tilemap address marks the tile dirty, but the tile data (graphics) might not have changed; or a single byte write marks the whole 8×8 tile dirty.

**FAILURE MODE 3 — Coherency with frame cache**
Mechanism: The existing frame cache (full-state hash) and dirty-tile path must agree. If dirty map says "clean" but frame hash differs, we get stale frames.

**ASSUMPTION**
SimCity's tilemap is linear in VRAM (standard SNES BG mode). If false: tilemap addressing mode (0x2105 bit 0-1) changes layout, dirty tracking breaks.

**ALTERNATIVE FRAMING**
Instead of tile-level dirty tracking, use VRAM page dirty bits (256-byte pages). Coarser but covers DMA/HDMA automatically.

**Porquê? ×5**
1. Why tile-level? → 8×8 tiles match BG rendering granularity
2. Why not page-level? → Page-level over-renders; tile-level is optimal
3. Why not full VRAM diff? → 64 KiB memcmp per frame is ~0.1ms, acceptable but less precise
4. Why not use existing frame cache? → Frame cache already does full-state hash; dirty tiles is for *partial* re-render when state changes
5. Why now? → T029 eliminated write overhead; render is now bottleneck

### Implementor Phase

**Como se fosse uma criança:** "A gente marca só os quadrados que mudaram e só redesenha eles. O resto fica como tava."

**Como se fosse um especialista:** Add a 16 KiB dirty-tile bitmap (1024 tiles × 1024 tiles / 8 = 131,072 bits = 16,384 bytes) to `SCV11Runtime`. On every VRAM write to tilemap region (0x0000-0x7FFF for BG1/BG2 tilemap), compute tile index (address / 32 for 32×32 tilemap, or address / 64 for 64×64) and set bit. In `sc_v28_render_first_visible_frame`, before processing a scanline, check if any tile on that line is dirty; if clean, skip layer processing for that line. At frame end, clear dirty bitmap.

**ADDRESSING FAILURE MODE 1**
Hook DMA writes in `sc_v11_run_dma_mask` (destination 0x2118/0x2119) to mark affected tiles dirty. Hook HDMA if used for VRAM.

**ADDRESSING FAILURE MODE 2**
Accept over-marking: marking whole 8×8 tile dirty on any byte write is safe (just slightly more render work). Precision is tile-level, not pixel-level.

**ADDRESSING FAILURE MODE 3**
Render path: if dirty map is empty → use frame cache (existing). If dirty map has bits → partial render only dirty tiles, then update frame cache hash. This keeps both systems consistent.

**ADDRESSING ASSUMPTION**
Verify SimCity uses mode 1 (32×32 tilemap) for BG1/BG2 during city view. Check 0x2105 register.

**ADDRESSING ALTERNATIVE FRAMING**
Start with tilemap dirty tracking (most impactful). If tile data (CHR) also changes, extend to CHR dirty tracking later.

**SOLUTION PROPOSAL**
1. Add `uint8_t vram_dirty_tiles[16384]` to `SCV11Runtime` (16 KiB)
2. In `sc_v11_bus_write8/16` and DMA path, if address in VRAM tilemap range (0x0000-0x7FFF), compute tile index and set bit
3. In `sc_v28_render_first_visible_frame`, for each scanline, compute which tiles are visible; if none dirty, skip layer
4. At frame end, `memset(dirty_tiles, 0, 16384)`
5. Integrate with existing frame cache: if dirty map empty → full cache check; if dirty → partial render + update cache

## Hostile Analysis

**ASSUMPTIONS I AM MAKING:**
- [KNOWN] SimCity uses BG mode 1 (16-color, 32×32 tilemap) for city view — verified by 0x2105=0x09 in connector
- [INFERRED] Tilemap at VRAM 0x0000-0x1FFF (BG1), 0x2000-0x3FFF (BG2) — standard SNES layout
- [ASSUMED] All VRAM writes during load go through bus write hook — DMA may bypass
- [UNKNOWN] Whether HDMA writes to VRAM during load

**WHAT WAS NOT SPECIFIED:**
- Exact VRAM tilemap base addresses per BG layer (need to read 0x2107-0x210A)
- Whether CHR (tile graphics) also changes during load

**ALTERNATIVES NOT CHOSEN:**
- Full VRAM memcmp per frame (0.1ms) — simple but doesn't enable partial render
- Page-level dirty (256-byte) — coarser, easier but less render savings

**RISKS AND SIDE EFFECTS:**
- 16 KiB extra RAM per runtime instance (acceptable)
- Complexity in render loop: per-scanline tile intersection test
- Must keep dirty map in sync with all VRAM write paths (bus, DMA, HDMA, PPU regs)

**COST OF BEING WRONG:** high — visual corruption, hard to debug. Mitigation: feature flag, fallback to full render.

**SCOPE BOUNDARY:**
In scope: Tilemap dirty tracking for BG1/BG2, integration with frame cache, bus/DMA write hooks.
Out of scope: CHR dirty tracking, HDMA hooking (if needed later), mode 7 support.

## Technical Approach

**Chosen approach:** Tilemap dirty bitmap + partial render integration

1. **Dirty bitmap:** 16 KiB array in `SCV11Runtime` (1024×1024 tiles / 8). Tile index = (y * 1024 + x) for 1024-wide logical tilemap; actual tilemap is 32×32 or 64×64, so map VRAM address to (tx, ty) via 0x2107-0x210A registers.

2. **Write hooks:**
   - `sc_v11_bus_write8/16` → if VRAM address in tilemap range, mark dirty
   - `sc_v11_run_dma_mask` → if DMA destination is 0x2118/0x2119, mark range dirty
   - `sc_v11_ppu_write_raw` for 0x2118/0x2119 already covered by bus write

3. **Render integration:**
   - In `sc_v28_render_first_visible_frame`, before layer loop, compute visible tile range for each scanline
   - If no dirty tiles in scanline's tile range for a layer, skip `background_pixel` calls for that layer
   - Track max dirty scanline to early-out if all remaining lines clean

4. **Frame end:** `memset(runtime->vram_dirty_tiles, 0, 16384)` in `advance_internal` after flush.

5. **Frame cache integration:** If dirty map empty → full cache check. If dirty → partial render, then recompute frame hash.

## Affected Files

| File | Operation | Description |
|------|-----------|-------------|
| static-recomp/internal/include/sc_v11_runtime.h | modify | Add `uint8_t vram_dirty_tiles[16384]` to `SCV11Runtime` |
| static-recomp/src/sc_v11_bus.c | modify | Mark dirty tiles on VRAM writes (bus, DMA) |
| static-recomp/src/sc_v28_video.c | modify | Partial render using dirty tile bitmap |
| static-recomp/src/simcity_static_recomp.c | modify | Clear dirty bitmap at frame end; integrate with frame cache |

## Specification

**New field in SCV11Runtime:**
```c
uint8_t vram_dirty_tiles[16384];  // 1024*1024/8 bits
```

**New helper functions in sc_v11_bus.c:**
```c
static inline void vram_mark_tile_dirty(SCV11Runtime *runtime, uint16_t vram_addr);
static inline void vram_mark_range_dirty(SCV11Runtime *runtime, uint16_t start, uint16_t len);
```

**Render interface change:**
- `sc_v28_render_first_visible_frame` receives dirty bitmap pointer
- Returns early for clean scanlines

## Testing Strategy

1. **Unit:** Dirty bit setting/clearing, tile index calculation
2. **Integration:** Load window profile — render ≤1ms/frame average
3. **Visual:** Compare frame hashes with/without dirty path (should match)
4. **Regression:** All ctest tests pass
5. **Stress:** Toggle dirty path on/off via env var, verify identical output

## Verification Criteria

- [ ] Load window (frames 300-900) average render ≤ 1 ms/frame
- [ ] Scrolling render ≤ 0.2 ms/frame (same as T029)
- [ ] `headless-test` 600 frames passes with identical frame hashes
- [ ] All ctest tests pass
- [ ] No visual artifacts in city map (manual check)
- [ ] Lint/build clean

## Estimation

- Complexity: high (8-16 hours)
- Risk: high
- Blocking dependencies: no

---

**Plan complete for T030.**
Output: `aes/tickets/T030-plan.md`
Next step: `/aes-build` (implement dirty-tile render path)