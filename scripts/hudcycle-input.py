#!/usr/bin/env python3
"""Emit the --input spec equivalent to widescreen_probe's
practice-hud-native-cycle route, so headless-profile can screenshot it.

The probe encodes its scenario as C functions of the frame number. Screenshots
are the missing half: the probe reports WRAM checkpoints but writes no images
for this mode, and the symptom under investigation is visual -- whether the
tool cursor is drawn where the toolbar is.

pulse(frame, start) in the probe is `start <= frame < start + 2`, reproduced
here as the same two frames. Overlapping presses are merged into one mask per
frame and a transition is emitted only where the mask actually changes, so the
sequence is unambiguous for --input.
"""
import argparse

ORDER = ["START", "SELECT", "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y"]


def build(upto):
    on = [set() for _ in range(upto)]

    def press(frame, btn):
        on[frame].add(btn)
        on[frame + 1].add(btn)

    def hold(a, b, btn):
        for f in range(a, b):
            on[f].add(btn)

    press(420, "START")                      # practice_input()
    for f in (540, 960, 1380):
        press(f, "B")
    for f in (1800, 2300):                   # practice_hud_native_cycle
        press(f, "SELECT")
    for f in (1840, 1860, 1900):
        press(f, "RIGHT")
    press(1880, "DOWN")
    for f in (1980, 2400):
        press(f, "B")
    for f in (2100, 2180):
        press(f, "X")
    return on


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upto", type=int, default=2500)
    args = ap.parse_args()

    on = build(args.upto)
    parts, previous = [], set()
    for frame, mask in enumerate(on):
        if mask == previous:
            continue
        previous = set(mask)
        parts.append(f"{frame}:{'+'.join(b for b in ORDER if b in mask) or '0'}")
    print(",".join(parts))


if __name__ == "__main__":
    main()
