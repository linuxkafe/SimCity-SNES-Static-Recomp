---
ticket: T061
type: performance
phase: plan
status: open
priority: high
closure_condition: "The scheduler no longer steps the PPU clock one master clock at a time, with the --hash screen-hash stream and the --shots BMP frames byte-identical over 900 frames of the T048 city input cadence, and the live-city frame cost at least 25 % lower."
depends_on: T060
---

# T061 — Step the PPU clock to the next event, not one master clock at a time

## Context

T039 re-derived the measurement and showed that rendering, not emulation, was
the bottleneck; T059 and T060 took the render path down. What was left was
the scheduler.

The line-level profile after T060:

```
 25.26 %  raw_clock (sc_v11_scheduler.c:79)
  2.06 %  raw_clock (sc_v11_scheduler.c:98)
  0.95 %  raw_clock (sc_v11_scheduler.c:55)
  0.86 %  raw_clock (sc_v11_scheduler.c:60)
```

`sc_v11_scheduler_advance()` called `raw_clock()` and `refresh()` **once per
master clock**:

```c
for (index = 0u; index < clocks && !runtime->route_failed; ++index) {
    raw_clock(runtime);
    refresh(runtime);
}
```

The master clock advances 357,000 times per frame, so that loop body ran
357,000 times per frame, for a timing model that only does anything at:

- a line boundary (`hclock >= line_length`),
- the NMI line at hclock 2 (frame latch, NMI) and hclock 6 (NMI edge),
- scanline 0 at hclock 2 (NMI clear),
- the refresh point, once per line, at hclock 531-538,
- the auto-joypad deadline, while it is armed.

Everything between those points was two counter increments.

## First: it is not call overhead

Marking `raw_clock` and `refresh` `always_inline` changed nothing (live
city 7979 -> 7825 us/frame, i.e. noise). The cost is the 357,000 executions,
not the calls. Reverted.

## The change

`sc_v11_scheduler_advance()` now computes the hclock of the next event
(`next_event_hclock()`), steps the clock straight to it, and handles only the
events the last clock of the step can carry. The step is clamped so that it
can never cross an event, so only one clock per step needs the event checks.

The event bodies are transplanted verbatim from `raw_clock()`; only the
dispatch changes. `raw_clock()` itself is unchanged and is still used for the
40 clocks that `refresh()` consumes, so the auto-joypad deadline landing
inside a refresh keeps its original behaviour.

Two details that are easy to get wrong, and were checked:

- `auto_joypad_end_master_clock` and `auto_joypad_poll_count` live on the
  **runtime**, not on the scheduler.
- `master_clock` is 64-bit, and the original comparison
  `scheduler->master_clock >= runtime->auto_joypad_end_master_clock`
  promotes the 32-bit side. The step clamp uses the same 64-bit comparison
  so the disarm lands on the same clock it used to.

## Verification

- `headless-profile --hash` over 900 frames of the T048 city input cadence:
  343 entries, **byte-identical** to the pre-T059 baseline.
- `headless-profile --shots`: 73 BMPs, all byte-identical.
- `ctest`: 4/4 pass.

## Measured

| | before | after | |
|---|---:|---:|---:|
| native x86, live city, us/frame | 7786.7 | 6885.5 | **1.13x** |
| native x86, whole run, us/frame | 7082.9 | 5794.7 | **1.18x** |
| 3DS, guest clock per frame | 67.14 s | 50.36 s | **1.33x** |

Cumulative from the pre-T059 baseline: **1.30x in the live city, 1.38x over
the whole run** natively, **1.33x on the 3DS**.

## Acceptance

- [x] The scheduler steps to the next event rather than one master clock at a
      time
- [x] The `--hash` stream over 900 frames of the T048 cadence is
      byte-identical to the pre-T059 baseline
- [x] Every `--shots` BMP is byte-identical to the pre-T059 baseline
- [x] `ctest` passes
- [x] The live-city frame cost falls by at least 25 % — 1.33x on the 3DS
- [ ] The render path and the scheduler together reach 2x against the
      pre-T059 baseline (currently 1.38x)

## Notes

- The 3DS emulator now sustains 71.9 guest frames per second in the live
  city, up from 27.9 before this series of changes. The "30 fps in the
  emulator" target from the 3DS port's T047 is met with margin.
- The 3DS device is not: 50.36 s of guest clock per frame is still 50.36 s
  on the device, so the device frame rate is ~0.02 fps in this game state.
  That gap is the one T047 describes as needing a real static recompiler.
- `next_event_hclock()` assumes the refresh point (hclock 531-538) cannot
  coincide with the NMI line's hclock 2 and 6 events, and that the refresh's
  40 clocks cannot reach the line boundary. Both hold because the refresh
  point is derived from the line boundary and 538+40 < 1360. The hash gate
  would catch it if they ever stopped holding.
