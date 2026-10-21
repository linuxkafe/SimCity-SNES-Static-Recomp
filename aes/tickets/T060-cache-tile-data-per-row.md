---
ticket: T060
type: performance
phase: plan
status: open
priority: high
closure_condition: "Render cost per frame in the live city view falls by at least 2x against the pre-T059 baseline, with the --hash screen-hash stream and the --shots BMP frames byte-identical over 900 frames of the T048 city input cadence."
depends_on: T059
reopens: T030
---

# T060 — Cache the tile data bytes per tile row, not per pixel

## Context

T059 hoisted the per-pixel work the per-line setup already did, and cached
the per-pixel *map* lookups per tile. It measured 1.13x native and 1.18x on
the 3DS, against T059's 2x closure condition, so it stays open.

The line-level profile after T059:

```
 30.37 %  background_pixel
 29.36 %  (gprof artefact: inlined raw_clock / refresh)
 15.27 %  render_lines_impl
  8.22 %  screen_pixel
```

The render path is still ~53 % of the frame. This ticket takes the next
step down the same path.

## What is still per pixel

After T059, the remaining per-pixel work inside `background_pixel()` is the
tile *data* fetch:

```c
row_address = char_base + (uint32_t)tile * (bpp * 8u) + py * 2u;
bit = 7u - px;
for (plane = 0u; plane < bpp; ++plane) {
    uint32_t address = row_address + (plane >> 1) * 16u + (plane & 1u);
    color |= (uint8_t)(((g_vram[address & 0xffffu] >> bit) & 1u) << plane);
}
```

`bpp` is 2 or 4, so this is **2-4 scattered byte reads from `g_vram` per
pixel per layer** — 8 to 16 per output pixel, and 89,152 output pixels in
the widescreen city view.

`g_vram` is 64 KiB. A scattered byte read across 64 KiB does not stay in a
32 KiB L1, so each one is an L2 access on the host and a cache miss on the
3DS's ARM11.

**But the bytes are the same for all eight pixels of a tile row.** Only `bit`
changes. The row is addressed by `char_base + tile * (bpp * 8) + py * 2`, and
within a line `char_base`, `bpp` and `py` are all constant, so the row is
determined by the effective `tile` alone.

## The change

Cache the `bpp` plane bytes per effective tile, in the same per-line setup
structs T059 introduced:

- `BgLineSetup` gains `td_valid` / `td_tile` / `td_bytes[4]`.
- `CityBgSetup` gains the same.
- `background_pixel()` and `city_background_pixel()` read the bytes from the
  cache when the effective tile matches, and refill it otherwise.

The key is the **effective** tile (after the 16x16 sub-tile adjustment), not
the raw tilemap entry, because that is what determines the row.

## Why this is not a cache in the "keep the rendered pixels" sense

This caches *tile data bytes*, not rendered pixels. It changes no semantics:
the same bytes are read, only once per tile row instead of once per pixel.
It is the same class of change as T059, and it carries none of the
dirty-marking correctness surface that made T030's original mechanism
unsound.

## Correctness gate

Unchanged from T059, and it inspects the final framebuffer rather than a
layer buffer (SD-PROCESS-005):

- `headless-profile --hash` over 900 frames of the T048 city input cadence
  (`scripts/headless-city-input.py --frames 900`) — byte-identical to the
  pre-T059 baseline.
- `headless-profile --shots DIR` over the same run — every BMP byte-identical.
- `ctest` 4/4.

## Acceptance

- [ ] The `--hash` stream over 900 frames of the T048 cadence is
      byte-identical to the pre-T059 baseline
- [ ] Every `--shots` BMP is byte-identical to the pre-T059 baseline
- [ ] Render cost per frame in the live city view falls by at least 2x
      against the pre-T059 baseline, measured on the 3DS
- [ ] `ctest` passes
- [ ] The T059 closure condition is met and T059 can close

## Notes

- The 3DS measurement matters more than the native one here: the win is
  specifically about cache behaviour, and the ARM11's 64 KiB D-cache with no
  second level for this working set should benefit more than the host's L1/L2
  hierarchy does.
- The `g_vram` working set is 64 KiB and the tile rows are scattered, so the
  bytes are not contiguous and cannot be fetched as a block.
