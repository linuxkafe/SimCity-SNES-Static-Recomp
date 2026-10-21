---
ticket: T062
type: performance
phase: plan
status: open
priority: high
closure_condition: "The hot scheduler state is in the first cache lines of SCV11Runtime, with the --hash screen-hash stream and the --shots BMP frames byte-identical over 900 frames of the T048 city input cadence."
depends_on: T061
---

# T062 — Put the hot state at the front of SCV11Runtime

## Context

T061 moved the scheduler from one master clock at a time to one event at a
time, which was 1.13x natively. But the 3DS barely moved, and the reason is
visible in the structure.

`SCV11Scheduler` is 48 bytes and it sat at the **end** of `SCV11Runtime`,
which is over 1.4 MiB -- the WRAM, the VRAM, the six PPU register counter
arrays and the two 240-entry scanline-state arrays are all in front of it.

The ARM11 this core targets has an 8 KiB I-cache and a 64 KiB D-cache. The
scheduler is read and written on every master clock, and the PPU renderer
streams 64 KiB of VRAM and 128 KiB of WRAM through the same cache between
frames. So the scheduler's cache lines were being evicted by the renderer
between frames and re-fetched 357,000 times per frame.

## The change

The members touched at master-clock rate are now first in the structure, so
they occupy the first few cache lines:

```c
typedef struct SCV11Runtime {
    SCV11Scheduler scheduler;
    uint16_t bg_scroll[8]; uint8_t bgofs_latch,bghofs_latch,oam_phase,cgram_phase;
    uint64_t auto_joypad_end_master_clock;
    uint32_t auto_joypad_poll_count;
    uint32_t ppu_completed_frame;
    uint16_t ppu_completed_line_count;
    uint16_t joypad[4];
    uint16_t auto_joypad_result[4];
    uint8_t open_bus; uint32_t wram_port;

    SCMachine machine;
    ...
```

There is no byte-level serialisation of this structure -- savestates go
through the VRAM/CGRAM/OAM/MMIO hash, not a raw dump -- so the layout was
free to change.

Confirmed in the linked 3DS ELF: the scheduler fields are now read at offsets
36-50 off the runtime pointer, and the bulk-advance logic is there.

## Verification

- `headless-profile --hash` over 900 frames of the T048 city input cadence:
  343 entries, **byte-identical** to the pre-T059 baseline.
- `headless-profile --shots`: 73 BMPs, all byte-identical.
- `ctest`: 4/4 pass.

## Measured, natively, by game segment

| segment | pre-T059 | T059-T061 | +T062 |
|---|---:|---:|---:|
| title screen | 7150.4 | 4838.3 (1.48x) | 4821.5 (1.48x) |
| map select | 7708.4 | 5538.6 (1.39x) | 5315.7 (1.45x) |
| live city | 8978.9 | 6885.5 (1.30x) | 6452.4 (1.39x) |
| whole run | 7984.0 | 5794.7 (1.38x) | 5563.8 (1.43x) |

## The 3DS did not move, and that is the interesting part

On the 3DS, four builds with very different cores report almost the same
`core` phase:

| 3DS build | core | present |
|---|---:|---:|
| simcity_t038.3dsx (pre-everything) | 2.503 s | 82.4 ms |
| simcity_t043_nonblock.3dsx | 2.503 s | 82.4 ms |
| simcity_t060_sched.3dsx (T059-T061) | 2.442 s | 82.4 ms |
| simcity_t064_layout.3dsx (+T062) | 2.487 s | 82.4 ms |

`present` is 82.4 ms -- five 60 Hz VBlanks -- to the millisecond in every
build. `core` moves by 2 %.

Natively the same work is 5,563 us (post) against 7,984 us (pre), and the
3DS is 2.5 s. That is a 350x ratio where the clock ratio alone accounts for
13x, so the ARM11 is running this code roughly 27x less efficiently per
instruction than x86-64 -- which is what a cache-miss-bound loop looks like,
not what an instruction-bound one looks like.

**Consequence: instruction-level optimisation does not transfer to the 3DS.**
Everything T059-T062 removed was instructions and redundant *reads*; the
3DS is waiting on memory. Making the working set fit the D-cache is the
lever on the device, and it is a different set of changes from the ones that
won on x86.

## Acceptance

- [x] The hot state is at the front of the structure
- [x] The `--hash` stream and the `--shots` BMPs are byte-identical
- [x] `ctest` passes
- [x] Measured natively, by segment
- [ ] A measurement that separates instruction cost from stall cost on the
      3DS, so the next change is aimed at the right thing

## Notes

- The 3DS numbers above are the guest's own tick clock, which on real
  hardware is wall-clock rather than work. That is why they are so much
  larger than the native ones and why they do not move when instructions are
  removed.
- See T063 for the consequence for the 30 fps target.
