# T046: Remaining cheats are unimplemented

## State after T045
Implemented and working:

| Cheat | Mechanism |
|---|---|
| ADD MONEY | one-shot write of 999999 to $0B9D |
| START $3,000 / $40,000 / $60,000 / $65,535 | one-shot write to $0B9D |
| FREEZE MONEY | writes 999999 every frame |
| MONEY NEVER DECREASES | tracks the highest value seen, rewrites $0B9D on a drop |

Still unimplemented (previously silent no-ops, now documented):

| Cheat | Why it is not implemented yet |
|---|---|
| EARN $1 PER BUILD | needs to observe building placement, which has no host-visible hook |
| TIME GOES FASTER | needs the game's time-rate variable |
| TIME GOES SLOWER | same |
| ALL BUILDINGS POWERED | needs per-building power state |
| DEMOLISH ANYTHING | needs to bypass the game's demolition validation |

## Why these are blocked
Each needs either a host-visible signal or the address of a game variable.
Finding them requires reaching live gameplay, because the relevant state is
only populated once a city exists.

The headless run cannot reach that state: funds at $0B9D never become
non-zero across 950 frames, and the documented button sequence stalls on a
static screen. Until gameplay is reachable headlessly, the addresses cannot
be discovered by observation.

## Multi-level time speed (user request)
Currently TIME FASTER and TIME SLOWER are booleans, and neither is wired.
A multi-level implementation needs:

1. the address of the game's time-rate value (or the tick rate it drives),
2. a decision on the level set, for example
   OFF / 1x / 2x / 4x / 8x with the existing two rows becoming a cycle,
3. a way to write it every frame so the game does not overwrite it.

Step 1 is the blocker. The mechanism in step 3 is the same frame-loop write
used by the money cheats, so once the address is known the rest is routine.

## Recommendation
Do not ship these toggles as if they work. Either remove them from the menu
or label them clearly, until gameplay can be reached and their state located.