#!/usr/bin/env python3
"""Emit an --input spec that drives the game from boot into a live city.

Reaching a city headlessly is what unlocks verification of everything that
only exists during gameplay: the camera, OAM sprites, the rail/train, and the
addresses the unimplemented cheats would need.

The game is walked forward by pressing B (the guest's confirm button) on a
cadence rather than by a hand-written button sequence. An earlier hand-written
sequence never advanced past the map-selection screen; the cadence works and is
deterministic, reaching funds at $0B9D by frame 573.

Usage:
    scripts/headless-city-input.py [--frames N] [--press-start F]

Prints an --input spec for headless-profile.
"""
import argparse

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=3000,
                    help="run length; the city is reached well before this")
    ap.add_argument("--period", type=int, default=20,
                    help="frames between presses")
    ap.add_argument("--hold", type=int, default=6,
                    help="frames each press is held")
    args = ap.parse_args()

    parts = []
    for f in range(30, args.frames, args.period):
        parts.append(f"{f}:b")
        parts.append(f"{f + args.hold}:0")
    print(",".join(parts))

if __name__ == "__main__":
    main()
