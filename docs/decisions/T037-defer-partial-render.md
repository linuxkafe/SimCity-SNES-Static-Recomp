# T037 Decision Record: T030 B6 Partial Render

## Context
T030 B6 (partial render via dirty-tile bitmap) was committed as infrastructure in f31a45a/71f197a:
- `SCV11Runtime.vram_dirty_tiles[16384]` bitmap (1 bit per 8×8 VRAM tile)
- `sc_v11_vram_mark_tile_dirty()` + flush path sets bits
- `sc_v28_render_first_visible_frame_columns()` + `render_lines_impl()` added for column-region rendering

But the render loop in `render_current_frame()` still does full-frame render unconditionally. The dirty bitmap is never read.

## Hostile Analysis

### Claim: "Partial render will significantly reduce render time during scrolling"
**Evidence against:**
- Current full render: ~6ms (headless profile, frames 750-950)
- Render is already sub-16ms; emulation (12.4ms) dominates
- Even reducing render to 0ms only saves 6ms → 12.4ms total = 80 fps (theoretical)
- But dirty-tile tracking has overhead: per-VRAM-write marking, bitmap scan, region stitching
- Region renderer processes ALL scanlines for selected columns; PPU pipeline per line is the cost
- Mode 0 BG layers (city view) cover most of screen; dirty tiles likely dense during scroll
- `analyze()` (4096 slots) runs per full frame; partial render would need per-region or skip

### Claim: "Dirty-tile bitmap accurately tracks changed pixels"
**Evidence against:**
- VRAM writes marked at tile granularity (8×8), but render reads VRAM per-pixel via tilemap indirection
- A single tilemap entry change affects 8×8 on screen; tile data change affects all instances
- Current marking: `sc_v11_vram_mark_tile_dirty(vram_addr)` called on VRAM write (tile data) and tilemap write
- But tilemap writes use MMIO 0x2116/0x2117 path; does it mark correctly?
- Scroll register changes (0x210D-0x2114) don't mark VRAM dirty but DO change visible content
- Widescreen `expand_city_view` uses WRAM camera, not scroll regs; dirty tracking blind to it

### Verification Gap
- No headless path reaches a live city with scrolling (funds 0x0B9D stays 0, sequence stalls)
- Cannot visually verify partial vs full render output
- No automated pixel-diff test exists for scrolling frames

### Opportunity Cost
- Implementing + verifying partial render: ~2-3 AES cycles (plan/build/verify/review/learn)
- Emulation optimization (0x09000 group): same effort, potential 5-8ms gain (30-60% of adv time)
- Removing `sc_v11_smp_sync_to_master` per advance already gave ~2ms; more APU batching possible
- `record_rendezvous` hashing ~3ms; could be thinned or cached

## Decision: DEFER / DESCARTAR T030 B6

**Verdict:** Partial render via dirty tiles is **NÃO-SUPORTADA** as a performance win for the current bottleneck. The render path is not the limiter; emulation is. The dirty-tile infra adds complexity without measurable benefit at current profile. 

**Action:** 
1. Keep region renderer (`sc_v28_render_first_visible_frame_columns`) as generic capability
2. Document dirty-tile bitmap as diagnostic/debug aid (can be dumped via headless-profile --watch)
3. Redirect effort to emulation hot path: group 0x09000 (VRAM write / tilemap init during city load)
4. Close T030 B6 with this rationale; open new ticket for emulation optimization if desired

## Acceptance Criteria for Reversal
If any of these become true, reopen:
- Headless verification reaches stable scrolling city (funds > 0 sustained)
- Render time exceeds 8ms sustained (currently 5-6ms)
- Emulation optimized below 8ms making render the new bottleneck
