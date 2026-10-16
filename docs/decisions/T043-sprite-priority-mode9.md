# T043: Train and other sprites draw over buildings (mode 9 priority fallback)

## Symptom (user-reported)
The train crosses the screen and renders over buildings and, apparently, over
any point of the screen.

## Root cause
`bg_priority()` in `static-recomp/src/sc_v28_video.c` handles modes 0, 1, 2-5
and 6 explicitly, then falls through to `return 0u` for every other mode.

The live city view is PPU **mode 9**: `expand_city_view` requires
`(current_reg(0x2105u) & 0x0fu) == 0x09u` (sc_v28_video.c:893), and mode 9 has
no entry in `bg_priority`. So in the city view:

- every background layer resolves to priority **0**
- `obj_priority()` has no mode-9 entry either and falls to `other[] = {2,4,6,8}`

`screen_pixel()` only draws the sprite when `priority > best.priority`, so the
sprite wins against **every** background layer at **every** OAM priority.

## Proof
A standalone reproduction of both tables, checking whether any object priority
exceeds any background priority:

```
mode 0: BG1=8 BG2=7 BG3=2 BG4=1 | OBJ0=3 OBJ1=6 OBJ2=9 OBJ3=12  -> always front: no
...
mode 7: BG1=0 BG2=0 BG3=0 BG4=0 | OBJ0=2 OBJ1=4 OBJ2=6 OBJ3=8  -> always front: YES (BUG)
mode 8: BG1=0 BG2=0 BG3=0 BG4=0 | OBJ0=2 OBJ1=4 OBJ2=6 OBJ3=8  -> always front: YES (BUG)
mode 9: BG1=0 BG2=0 BG3=0 BG4=0 | OBJ0=2 OBJ1=4 OBJ2=6 OBJ3=8  -> always front: YES (BUG)
```

## Secondary finding
The mode-0 tables also disagree with the documented SNES hardware ordering.
Hardware mode 0 is BG1 = 0, BG2 = 4, BG3 = 8, BG4 = 12 (each raised by one
when the tilemap entry's high bit is set), with objects at 0/2/6/10. The code
has:

```
mode0 bg:    {8,11},{7,10},{2,5},{1,4}      /* BG1 highest, BG4 lowest: reversed */
mode0 obj:   {3,6,9,12}
```

The same inversion appears in mode 1. This is a second, independent ordering
error. It is **not** what causes the reported train bug, because the city view
never reaches the mode-0 path, but it will misorder layers wherever mode 0 is
used (title, menus, HUD).

## Why the existing tests did not catch this
`simcity-frame-colors` and `simcity-color-ground-truth` compare colours and
channel order, and the hash comparisons used for the T040-T042 optimisations
compare a build against **itself**. None of them encode an independent
expectation about layer ordering, so a self-consistent wrong ordering passes.

## Proposed fix
Give modes 7-9 explicit, hardware-faithful priority tables rather than letting
them fall through to 0, and correct the reversed mode-0/mode-1 tables.

## Verification limit (stated up front)
The fix cannot be visually confirmed in this environment: the headless run
never reaches a live city (funds at $0B9D never become non-zero), and the
agent cannot view images. Verification available here is:
1. the standalone table reproduction above,
2. bit-level regression testing to confirm no *unintended* pixel change on
   frames the benchmark does cover,
3. the user's own report on real hardware as the final gate.