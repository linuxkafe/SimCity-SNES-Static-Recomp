---
ticket: T029
phase: plan
status: done
created: 2026-10-15
tier: standard
requires:
  - aes/kanban.md
  - aes/tickets/T029-batch-vram-writes.md
produces:
  - aes/tickets/T029-plan.md
blocked_by: ''
---

# T029 — Plan: Batch VRAM writes during city load

## Reconnaissance Summary

**Problem:** PC-group profile shows group **0x09000** (generated town route shard `sc_v35_group_00024.c`) accounts for ~89% of cycles during city load (frames 300-900). This code performs VRAM tile-map initialization via thousands of individual `sc_v11_bus_write8`/`sc_v11_bus_write16` calls.

**Current VRAM write path:**
- `sc_v11_bus_write8` → `sc_v11_write_raw` (per byte)
- `sc_v11_bus_write16` → two `sc_v11_bus_write8` calls
- Each call incurs function call overhead, address masking, cycle accounting

**Target generated code:** `static-recomp/generated/v35-shards/sc_v35_group_00024.c` (addresses 0x09058–0x09182) — town route initialization that writes tile data to VRAM.

**Files to modify:**
- `static-recomp/src/sc_v11_bus.c` — add write buffering
- `static-recomp/internal/include/sc_v11_bus.h` — new API if needed
- Generator (if exists) — for proper fix, but runtime buffer is faster path

## Two-Agent Analysis (not heavy tier)

Skipped — this is a standard optimization, not architectural.

## Hostile Analysis

**ASSUMPTIONS I AM MAKING:**
- [KNOWN] VRAM writes during load are sequential (tile-map data written linearly) — profiler shows single hot group
- [INFERRED] Game does not read back VRAM immediately after each write during init — typical for bulk tile upload
- [ASSUMED] Sequential writes can be buffered and flushed with memcpy — 10-100x speedup expected
- [UNKNOWN] Whether any VRAM read occurs mid-sequence — could break batching

**WHAT WAS NOT SPECIFIED:**
- Exact VRAM address range being written
- Whether DMA is also used for VRAM (separate path)

**ALTERNATIVES NOT CHOSEN:**
- Modify static recompiler generator — correct but requires rebuilding all generated code
- Add DMA-style batch API to generated code — requires re-generation

**RISKS AND SIDE EFFECTS:**
- Buffer flush on frame boundary must not exceed frame budget
- If game reads VRAM mid-sequence, stale data returned — need read-path invalidation
- DMA writes to VRAM bypass this path — must also be buffered or flushed

**COST OF BEING WRONG:** medium — visual corruption during load, but recoverable after first frame

**SCOPE BOUNDARY:**
In scope: runtime write buffer in `sc_v11_bus.c` for sequential VRAM writes
Out of scope: generator changes, DMA path, VRAM reads

## Technical Approach

**Chosen approach:** Runtime write buffer in `sc_v11_bus.c`

1. Add a small circular buffer (e.g., 4 KiB) for pending VRAM writes
2. Track `last_write_addr` and `buffer_len`
3. On sequential write (addr == last + 1), append to buffer
4. On non-sequential write or frame boundary, flush buffer with `memcpy` to VRAM
5. Flush buffer at end of each frame in `simcity_recomp_advance_headless` / main loop
6. VRAM reads (`sc_v11_bus_read8/16`) must flush buffer first for coherence

**Buffer design:**
```c
#define VRAM_WRITE_BUFFER_SIZE 4096
struct VramWriteBuffer {
    uint32_t base_addr;
    uint16_t len;
    uint8_t data[VRAM_WRITE_BUFFER_SIZE];
    bool active;
};
```

**Flush condition:** non-sequential address, buffer full, or explicit frame-end flush.

## Affected Files

| File | Operation | Description |
|------|-----------|-------------|
| static-recomp/src/sc_v11_bus.c | modify | Add write buffer, modify write8/write16, add flush function |
| static-recomp/internal/include/sc_v11_bus.h | modify | Declare flush function, buffer struct if exposed |
| static-recomp/src/simcity_static_recomp.c | modify | Call flush at frame boundary in advance path |
| static-recomp/src/sc_v11_cpu.c | modify | Ensure VRAM reads flush buffer first |

## Specification

**New functions:**
- `void sc_v11_vram_write_flush(SCV11Runtime *runtime)` — flush pending writes
- `static inline void vram_buffer_append(SCV11Runtime *, uint32_t addr, uint8_t value)` — internal

**Behavior:**
- Sequential writes (addr == last_addr + 1) → buffered
- Non-sequential → flush, then buffer new
- Frame end → flush
- VRAM read → flush first

## Testing Strategy

1. **Unit:** Buffer append/flush logic with sequential/non-sequential patterns
2. **Integration:** Run load window profile (`--window 300:900`) — verify render ≤1 ms/frame
3. **Regression:** `ctest --test-dir build` — all tests pass
3. **Visual:** Verify no corruption in city map after load

## Verification Criteria

- [ ] Render time in load window (frames 300-900) averages ≤ 1 ms/frame
- [ ] `headless-test` 600 frames passes
- [ ] All ctest tests pass
- [ ] No visual artifacts in city map (manual check)
- [ ] Lint/build clean

## Estimation

- Complexity: medium (2-4 hours)
- Risk: medium
- Blocking dependencies: no

---

**Plan complete for T029.**
Output: `aes/tickets/T029-plan.md`
Next step: `/aes-build` (implement the write buffer)