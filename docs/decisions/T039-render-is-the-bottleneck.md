# T039: Measurement-validity correction — the render path is the bottleneck

## Trigger
AES verify step for the T033/T036 fixes required re-measuring to confirm the
stutter was gone. Re-measurement produced numbers that contradicted the model
recorded in T037, so the model was re-derived from evidence rather than kept.

## Prior (incorrect) model — T037
> "the render path is not the limiter; emulation is" — derived from
> `headless-profile`'s printed line `adv 12.2ms + render 5.9ms`.

## Evidence gathered

### 1. gprof (`build-prof`, `-pg -O2`) over 300 frames
```
  %   cumulative   self              self     total           calls
 51.45      1.77     1.77 51609600     background_pixel
 20.35      2.47     0.70 107159606    sc_machine_nonzero_wram   <- mis-attributed
  7.85      2.74     0.27      358     render_lines_impl
  3.78      2.87     0.13 17203200     screen_pixel
  2.33      2.95     0.08  3798805     sc_v11_scheduler_code_access
```
- `render_lines_impl` = **64.5 % cumulative**, invoked only 358 times.
- `screen_pixel` 17.2 M calls and `background_pixel` 51.6 M calls, **all from rendering**.
- The 20.3 % at `sc_machine_nonzero_wram` is a gprof symbol artifact. That function
  is a plain 128 KiB WRAM count used only by diagnostics and is never called from
  the hot path; the samples belong to `raw_clock`/`refresh`, which are `static`
  and were inlined, so gprof mapped them onto the next global symbol.

### 2. Direct measurement of pure emulation
A harness on `simcity_recomp_advance_headless()` (which passes `render_frame = 0`):

```
PURE EMULATION (advance_headless, no render): 3570 us/frame over 950 frames
```

### 3. Why the printed split was wrong
`simcity_recomp_advance()` calls `advance_internal(..., render_frame = 1, ...)`
and the render happens **inside** it (`simcity_static_recomp.c:840`). `headless-profile`
then calls `simcity_recomp_render_current_frame()` a second time, which hits the
full-state frame cache and costs almost nothing. So the label `render` was really
"cached second render", and the label `adv` actually contained the entire real render.

`--no-render` confirms this: total collapses to 12184 µs/frame with `render 0.0`,
identical to the previous `adv` figure.

## Corrected model

| Component | Cost |
|---|---|
| Pure emulation | **3.57 ms/frame** (~280 fps) |
| Rendering | **~8.6 ms/frame** |
| Total | ~12.2 ms/frame (~82 fps) |

Rendering is **~71 % of the frame**. It runs 224 scanlines × output_width pixels,
calling `screen_pixel` → `background_pixel` three times per pixel (main layer,
sub layer, colour windows).

## Consequences

1. **T037 is partly wrong.** Its conclusion "render is not the limiter" is false.
   The dirty-tile partial render it deferred is in fact the highest-value
   optimisation available. T037 must be reopened.
2. The T033 throttle removal was still correct — it discarded frames that had
   genuinely changed, which is the visible judder — but its *justification*
   referenced a render cost that is real (8.6 ms), just mis-labelled.
3. The earlier ablation figures quoted as "adv ~13 ms → ~19 ms emulation" conflated
   emulation with render and should be re-measured before reuse.

## Verdict
**SUPORTADA** — rendering is the bottleneck. Reopen T030 B6 with correct premises.