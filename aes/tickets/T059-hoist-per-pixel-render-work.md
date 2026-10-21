---
ticket: T059
type: performance
phase: plan
status: open
priority: high
closure_condition: "Render cost per frame in the live city view falls by at least 2x, with the --hash screen-hash stream and the --shots BMP frames byte-identical to the pre-change baseline over 900 frames of the T048 city input cadence."
reopens: T030
supersedes_premise_of: T037
---

# T059 — T030 B6: hoist the per-pixel work the per-line setup already did

## Why this reopens T030 B6

T037 deferred T030 B6 (dirty-tile partial render) on the premise that "the
render path is not the limiter; emulation is". **T039 measured that premise
false** and superseded T037: pure emulation is 3.57 ms/frame, rendering is
~8.6 ms/frame, and rendering is ~71 % of the frame.

T037's own reversal criteria are now met:

- *Render time exceeds 8 ms sustained* → **met** (8.6 ms).
- *Emulation optimized below 8 ms making render the new bottleneck* → **met**
  (3.57 ms).
- *Headless verification reaches a live city* → **met** (T048).

So T030 B6 is reopened. But **not with T030's original mechanism**, for the
reason T037 itself recorded and did not resolve: the city map and the city
camera live in **WRAM** (`SC_CITY_MAP_WRAM_OFFSET`, `SC_CITY_VIEW_X_OFFSET`),
not in VRAM. A VRAM dirty-tile bitmap is therefore blind to camera movement
and to map changes, and a renderer that consults it would emit a stale city.

## What the profile actually points at

Measured on the Deck with `headless-profile` (native x86) and gprof, live city
view, 600 frames:

```
 28.23%  background_pixel              72,769,536 calls
 16.09%  render_lines_impl                     694 calls
  6.07%  screen_pixel                   34,349,056 calls
  0.53%  sc_v11_cpu_step                 7,373,138 calls
```

The CPU emulation is ~2-3 %. The cost is in the per-pixel background
evaluation, and the reason is not that the work is necessary — it is that
**the per-line setup computes values that the per-pixel code then recomputes
from scratch.**

`bg_line_setup()` already computes, once per line per layer:

| field | source |
|---|---|
| `bgmode` | `view_reg(view, 0x2105)` |
| `mosaic_size` | `(view_reg(view, 0x2106) >> 4) + 1` |
| `mosaic_enable` | `view_reg(view, 0x2106) & (1 << layer)` |
| `world_width` / `world_height` | `map_width * tile_size` |

and `background_pixel()` then re-reads `0x2105` and `0x2106` **twice** per
pixel per layer, and re-derives `mode` the same way — while
`mosaic_size`, initialised from `bg->mosaic_size` at the top of the function,
is **overwritten** by the recomputed value a few lines later. The field was
already correct; the recomputation discards it.

Counted per output pixel in the city view (`city_screen_pixel`):

```
  1  view_reg(0x2105)                  in city_screen_pixel
+ 2  view_reg(0x2105)                  in city_background_pixel  (x2 layers)
+ 2  view_reg(0x2105)                  in background_pixel       (x2 layers)
+ 2  view_reg(0x2106) x2 each          in background_pixel       (x2 layers)
= 11 register reads per output pixel
```

plus one **signed 32-bit division** per pixel per layer, from
`wrap_coordinate(sample_x + scroll, world_width)`.

`world_width` is `map_width * tile_size` with `map_width` in {32, 64} and
`tile_size` in {8, 16} — always a power of two. The **existing** code already
masks `world_y` with `& (world_height - 1)` for exactly that reason, so the
`world_x` division is an inconsistency rather than a requirement.

## The change

1. `background_pixel`: take `mode` from `bg->bgmode & 7u`; take
   `mosaic_size` / `mosaic_enable` from `bg->`; delete the recomputation.
2. `city_background_pixel`: take `mode` and the mode-bit-3 flag from a new
   per-line `bgmode` field in `CityLineSetup`.
3. `screen_pixel` / `city_screen_pixel`: take `mode` from the caller.
4. `background_pixel`: replace the `world_x` modulo with a mask, matching the
   `world_y` treatment, guarded by an explicit power-of-two check.

**No dirty tracking. No cache. No change to what is rendered** — the same
values are read, just once per line instead of once per pixel. The output must
be bit-identical, and that is the gate.

## Correctness gate

Per upstream lesson SD-PROCESS-005 ("a regression gate that inspects the
wrong buffer reports green") the gate must inspect the **final framebuffer**,
not a layer buffer:

- `headless-profile --hash` over 900 frames with the T048 city input cadence
  (`scripts/headless-city-input.py --frames 900`) — the screen-hash stream
  must be byte-identical to the baseline. This is the same instrument T048
  used to prove that a button sequence did nothing.
- `headless-profile --shots DIR` over the same run — every BMP must be
  byte-identical to the baseline.

The baseline was captured before any edit: 343 hash lines and the screenshot
set, from the unmodified source.

## Why this rather than dirty tiles

Dirty-tile tracking is the right *eventually*, but it carries the
correctness risk T037 enumerated (missed dirty marks through DMA/HDMA, and
coherency with the frame cache), and it is blind to the WRAM city map.
Hoisting per-pixel reads is:

- **output-identical by construction** — the values are unchanged, only read
  once per line;
- **free of the dirty-marking correctness surface** entirely;
- **local to two functions** in one file.

If this lands and the render cost is still the limiter, dirty tiles (or a
tile cache) become the next step, with the same gate.

## Acceptance

- [x] The `--hash` stream over 900 frames of the T048 cadence is
      byte-identical to the baseline
- [x] Every `--shots` BMP is byte-identical to the baseline
- [ ] Render cost per frame in the live city view falls by at least 2x
      — **measured 1.13x native, 1.18x on the 3DS. Not met.**
- [x] gprof shows `background_pixel` and `screen_pixel` materially reduced
      — `background_pixel` self time 2.43 s -> 1.81 s (1.34x)
- [x] The existing test suite still passes (ctest 4/4)
- [x] The change is measured on the 3DS via the headless Azahar build, not
      only natively

## Measured

Live city, frames 573-900, same ROM and same input cadence:

| | before | after | |
|---|---:|---:|---:|
| native x86, us/frame | 8978.9 | 7979.4 | **1.13x** |
| 3DS, guest clock per frame | 67.1 s | 57.1 s | **1.18x** |

Committed as `0c31e4e`.

## The closure condition is not met, and the re-profile says why

The render path is no longer where the time is. The largest remaining single
item in the line-level profile is now:

```
 24.16 %  raw_clock (sc_v11_scheduler.c:79)
```

`sc_v11_scheduler_advance()` calls `raw_clock()` and `refresh()` **once per
master clock** — 357,000 times per frame — for a PPU timing model that only
does anything at:

- a line boundary (`hclock >= line_length`),
- the NMI line at hclock 2 and 6,
- scanline 0 at hclock 2,
- the auto-joypad deadline (only while auto-joypad is active).

Everything between those points is a counter increment. The line-level
profile also shows `raw_clock` lines 55/60/61/98 at a further ~4 %, with
`capture_ppu_scanline_state` (240 calls/frame) and `latch_completed_ppu_frame`
(240 struct copies/frame) inside it.

**The next target is the scheduler, not the renderer.** A bulk advance that
jumps to the next interesting boundary instead of stepping one master clock at
a time is the obvious shape of the fix. It is a bigger change than this one
and carries real correctness risk (the NMI edge counter, the auto-joypad poll
deadline, and `refresh()`'s exact semantics all live in that loop), so it
wants its own ticket with its own gate rather than being folded in here.

## Notes

- The fixes live here, in the upstream. The 3DS port consumes this repo
  read-only and cannot fix it.
- `world_width` being a power of two is an invariant of the SNES BG geometry,
  not of this game. The change must assert it rather than assume it, and
  fall back to the division if it ever fails. — done: the mask is applied
  only when `bg_line_setup()` has verified it, and the modulo is kept as the
  fallback.
- The per-tile caches live in the per-line setup structs, so they are reset
  once per line by `bg_line_setup()` / `city_line_setup()`. The main and sub
  screens sample the same background position, so the sub screen's lookup
  hits the cache the main screen just populated.
