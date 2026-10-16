# T047: Train offset from the map in widescreen

## Symptom
After T044 the train no longer draws over buildings. A remaining defect: when
the track runs vertically, the train appears to run about ten blocks to the
left.

## Leading hypothesis: sprites are not shifted with the map in widescreen

In `render_lines_impl`, when `expand_city_view` is active (the live city view):

- the map is re-derived from the WRAM camera, and the map's world origin
  (world x = 0) is placed at screen x = left_margin:
  `display_left_x = native_camera_x - left_margin`, then
  `city_source_x = display_left_x + x - native_camera_x`, which simplifies to
  `x - left_margin` while the camera is unclamped;
- `object_left_margin` is forced to **0**, so OAM sprites are drawn at their
  raw X;
- the only sprite that receives a shift is the map cursor, via
  `cursor_shift_x`.

So the map is displaced by `left_margin` relative to the non-widescreen render
while every other sprite is not. `SIMCITY_RECOMP_WIDESCREEN_MARGIN` is **71**
pixels. 71 / 8 = **8.875 blocks per city cell**, which matches the reported
"about ten blocks to the left" closely enough to be the strongest candidate.

A constant leftward offset of the train relative to the track it is on is
exactly the signature this predicts, and it would explain why the defect only
became noticeable now: before T044 the train drew on top of the map, so a
misregistration against the map was not apparent.

## What would confirm it
1. Turn **widescreen off** in the settings menu. If the offset disappears, the
   map/sprite registration in `expand_city_view` is the cause.
2. Check whether the offset changes as the camera is clamped at the left or
   right edge of the map. A margin-only offset stays constant; a
   camera-dependent one changes at the edges.
3. Check whether the on-screen cursor also sits off the map, since it is
   shifted by a different term (`cursor_shift_x`).

## Proposed fix (not yet applied)
Give every OAM sprite the same displacement as the map, i.e. shift sprites by
`left_margin + (native_camera_x - display_left_x)` when `expand_city_view` is
active, and leave the non-widescreen path untouched. The camera-dependent term
is required because `display_left_x` is clamped at both map edges.

## Verification limit
This is a hypothesis, not a proven root cause. It cannot be confirmed in this
environment because the headless run never reaches a live city, where the
camera and the OAM positions are first populated. The three checks above are
cheap and discriminate the hypotheses; the fix should not be applied until at
least check 1 comes back negative.