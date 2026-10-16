# T048: Reaching a live city in headless mode

## Why this was blocking
Everything that only exists during gameplay was unverifiable in this
environment: sprite and train placement, the camera, the cursor registration in
the widened view, and the game variable addresses that the remaining cheats
would need. Every attempt so far stalled on the map-selection screen with funds
at $0B9D stuck at 0.

The blocker was not capability, it was observability. `headless-profile` has
always had `--ascii`, which renders the frame as text. That had never been used
for navigation. With it, the screens became legible: frame 200 is a
map-selection grid, for example.

## The failed approach
A hand-written button sequence (`start -> down -> z -> ...`) was carried over
from earlier notes. Testing it against frame hashes showed that *no* button
changed the screen at all:

```
baseline (no input)      screen: frame 398 hash 1C478489
200:down,230:0           screen: frame 398 hash 1C478489
200:a,230:0              screen: frame 398 hash 1C478489
200:start,230:0          screen: frame 398 hash 1C478489
200:select,230:0         screen: frame 398 hash 1C478489
```

Full-stream comparison confirmed the streams were identical, not just the final
frame. So the recorded sequence was simply wrong for this game, not broken.

## What works
Press B (the guest confirm button) on a cadence instead of following a fixed
path: every 20 frames from frame 30, held for 6 frames.

```
wr 0   0x0B9D=0x0000
wr 573 0x0B9D=0x4E20     funds = 20000, the default starting cash
wr 573 0x01BD=0x002F     camera view X
wr 573 0x01BF=0x0022     camera view Y
```

Funds and camera both become live at **frame 573**, reproducibly across runs.

Emitted by `scripts/headless-city-input.py`, so the sequence is a tool rather
than a note in a ticket.

## Why a cadence beats a hand-written path
The exact screen sequence depends on timing that varies with how long each
screen dwells before accepting input. A cadence makes progress wherever the
game currently is, which is robust; a fixed script silently does nothing when
the timing assumption is wrong, which is what happened here for many attempts.

## What this unblocks
- Verifying the T047 sprite/map registration with real sprites on screen.
- Locating the time-rate, power and demolition state for the remaining cheats.
- Measuring render cost during an actual city view, where the per-pixel work is
  representative rather than being title screens.
