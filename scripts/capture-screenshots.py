#!/usr/bin/env python3
"""Capture README screenshots of the running frontend on a virtual display.

The README needs pictures of the game and of the settings overlay, and those
can only come from the real SDL frontend: headless-profile renders at core
resolution with no presentation path and cannot draw the menu at all.

Runs the frontend on Xvfb, drives it with synthetic XTEST key events, and grabs
the root window with ffmpeg. Key injection is necessary because the menu opens
on F1 and the frontend has no scripting hook -- adding one purely so the README
could be photographed would put test scaffolding into shipped code.

The window is located by scanning the captured framebuffer for its bounding
box rather than by asking X11. SDL's window placement without a window manager
is not predictable, and querying it through ctypes segfaulted; measuring the
picture is both simpler and honest.

Usage:
  scripts/capture-screenshots.py --rom ROM.sfc --out docs/images
"""
import argparse
import ctypes
import ctypes.util
import os
import shutil
import subprocess
import sys
import tempfile
import time

# X11 virtual keycodes, from keysymdef.h.
KEYSYM = {
    "F1": 0xFFBE,
    "Escape": 0xFF1B,
    "Return": 0xFF0D,
    "Down": 0xFF54,
    "Up": 0xFF52,
    "Left": 0xFF51,
    "Right": 0xFF53,
    "z": 0x007A,   # guest B (confirm)
    "x": 0x0078,   # guest A (cancel)
}


class Keys:
    """Synthetic key injection through the XTEST extension.

    XTEST lives in libXtst, not libX11; loading the wrong one fails with an
    undefined-symbol error rather than anything readable.
    """

    def __init__(self, display_name):
        lib_x11 = ctypes.util.find_library("X11")
        lib_xtst = ctypes.util.find_library("Xtst")
        if not lib_x11 or not lib_xtst:
            raise RuntimeError("libX11 and libXtst are both required")
        self.x11 = ctypes.cdll.LoadLibrary(lib_x11)
        self.xtst = ctypes.cdll.LoadLibrary(lib_xtst)
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.display = self.x11.XOpenDisplay(display_name.encode())
        if not self.display:
            raise RuntimeError(f"cannot open X display {display_name}")

    def _code(self, name):
        self.x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        return self.x11.XKeysymToKeycode(self.display, KEYSYM[name])

    def tap(self, name, hold=0.05, gap=0.12):
        key = self._code(name)
        self.xtst.XTestFakeKeyEvent(self.display, key, 1, 0)
        self.x11.XFlush(self.display)
        time.sleep(hold)
        self.xtst.XTestFakeKeyEvent(self.display, key, 0, 0)
        self.x11.XFlush(self.display)
        time.sleep(gap)


def content_bbox(raw, width, height):
    """Bounding box of non-black pixels: the window against the empty root."""
    stride = width * 3
    rows = [y for y in range(height)
            if any(raw[y * stride:(y + 1) * stride])]
    if not rows:
        return None
    y0, y1 = rows[0], rows[-1]
    cols = []
    for x in range(width):
        hit = False
        for y in (y0, y1, (y0 + y1) // 2):
            base = y * stride + x * 3
            if raw[base] or raw[base + 1] or raw[base + 2]:
                hit = True
                break
        if hit:
            cols.append(x)
    if not cols:
        return None
    return cols[0], y0, cols[-1] - cols[0] + 1, y1 - y0 + 1


def grab(display, screen, out_png, tmpdir):
    """Grab the root window and trim it to the window's bounding box."""
    sw, sh = screen
    raw = os.path.join(tmpdir, "grab.rgb")
    done = subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-f", "x11grab",
         "-video_size", f"{sw}x{sh}", "-i", display, "-frames:v", "1",
         "-pix_fmt", "rgb24", "-f", "rawvideo", raw],
        capture_output=True)
    if done.returncode != 0:
        sys.stderr.write(done.stderr.decode("utf-8", "replace"))
        return False

    with open(raw, "rb") as handle:
        buf = handle.read()
    box = content_bbox(buf, sw, sh)
    if box is None:
        return False
    x, y, w, h = box

    crop = subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo",
         "-pix_fmt", "rgb24", "-s", f"{sw}x{sh}", "-i", raw,
         "-vf", f"crop={w}:{h}:{x}:{y}", "-frames:v", "1", out_png],
        capture_output=True)
    if crop.returncode != 0:
        sys.stderr.write(crop.stderr.decode("utf-8", "replace"))
        return False
    print(f"  {os.path.basename(out_png)}  {w}x{h} at ({x},{y})")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", required=True)
    ap.add_argument("--out", default="docs/images")
    ap.add_argument("--screen", default="1400x1000",
                    help="virtual screen; must be larger than the window")
    ap.add_argument("--no-widescreen", dest="widescreen",
                    action="store_false", default=True)
    ap.add_argument("--settle", type=float, default=4.0,
                    help="seconds to wait after each key")
    args = ap.parse_args()

    for tool in ("Xvfb", "ffmpeg"):
        if not shutil.which(tool):
            sys.exit(f"{tool} is required")

    sw, sh = (int(v) for v in args.screen.lower().split("x"))
    os.makedirs(args.out, exist_ok=True)
    tmpdir = tempfile.mkdtemp(prefix="simcity-shots-")
    display = f":{os.getpid() % 90 + 100}"

    xvfb = subprocess.Popen(
        ["Xvfb", display, "-screen", "0", f"{sw}x{sh}x24", "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(1.5)
        env = dict(os.environ, DISPLAY=display, SDL_AUDIO_DRIVER="dummy")
        cmd = ["build/frontend/linux/simcity-linux", "--rom", args.rom,
               "--resolution", "1"]
        if args.widescreen:
            cmd.append("--widescreen")
        app = subprocess.Popen(cmd, env=env,
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
        try:
            time.sleep(4.0)
            keys = Keys(display)

            # Walk the boot screens to a live city. The cadence mirrors what
            # scripts/headless-city-input.py proves works headlessly: START,
            # then B on a repeating cadence.
            keys.tap("Return", gap=2.0)
            for _ in range(14):
                keys.tap("z", gap=0.55)
            # The tutorial's mayor panel sits over the city and hides the very
            # thing worth photographing, so keep confirming until it clears.
            for _ in range(18):
                keys.tap("z", gap=0.5)
            time.sleep(args.settle)

            grab(display, (sw, sh), os.path.join(args.out, "city.png"), tmpdir)

            keys.tap("F1"); time.sleep(args.settle)
            grab(display, (sw, sh),
                 os.path.join(args.out, "menu-settings.png"), tmpdir)

            # The menu opens with RESUME selected and navigation wraps, so CHEATS
            # is 8 steps away: RESUME -> QUIT -> RESOLUTION -> WIDESCREEN ->
            # HUD SHIFT X -> SOFT MOUSE -> MOUSE SENS -> COLOR -> CHEATS.
            for _ in range(8):
                keys.tap("Down", gap=0.18)
            keys.tap("z"); time.sleep(args.settle)
            grab(display, (sw, sh),
                 os.path.join(args.out, "menu-cheats.png"), tmpdir)
        finally:
            app.terminate()
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()
    finally:
        xvfb.terminate()
        shutil.rmtree(tmpdir, ignore_errors=True)

    print(f"\nscreenshots in {args.out}/")


if __name__ == "__main__":
    main()