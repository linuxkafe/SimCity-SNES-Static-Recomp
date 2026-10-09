# SimCity SNES Static Recomp 1.4.0

Native static recompilation core for SimCity on the Super Nintendo Entertainment
System. Windows and Linux frontends are supported; the core builds on both
platforms.

The original game ROM is not included. Use an externally supplied, legally
obtained ROM matching `ROM-REQUIREMENTS.txt`.

## What fully static recompilation means

SimCity's executable W65C816 game instructions were analysed and translated
into generated native C before the application was built. Production dispatch
uses exact processor-state and program-counter contexts. It contains no
general-purpose SNES CPU interpreter, runtime opcode decoder, dynamic
recompiler, JIT, or automatic emulator fallback.

The audio program is static too. SimCity's exact S-SMP program-counter/opcode
authority dispatches only compiled SPC700 instruction forms. Its protected-code
bitmap rejects writes to statically owned driver code, and unknown execution or
semantic ARAM reads stop with a diagnostic. The former emulator S-DSP runtime
has been removed. A project-owned fixed 32-phase S-DSP implements register
timing, BRR decoding, Gaussian interpolation, envelopes, noise, pitch
modulation, echo/FIR and signed 16-bit stereo PCM at the native 32,040 Hz rate.

Game simulation, graphics, samples, music and effects are not prerecorded.
They remain live state driven by the external ROM, cartridge memory, WRAM,
VRAM, ARAM, controller input and SNES timing.

The complete runtime includes:

- Generated exact-context W65C816 game-code dispatch.
- Native cartridge, WRAM, bus, DMA, HDMA, controller and CPU-I/O handling.
- Native PPU registers, scanline events, rendering and framebuffer output.
- Exact rational NTSC scheduling and one guest frame per host deadline.
- Fail-closed exact-PC S-SMP AOT and project-owned 32-phase S-DSP.
- Native 32,040 Hz stereo PCM with knownness, overflow and hash diagnostics.
- Battery SRAM and deterministic snapshots including continuing audio state.
- A Windows launcher (Win32/GDI, SDL3 gamepad, DirectSound) and a Linux
  frontend (SDL2, windowed) with gamepad support, arbitrary window sizing and
  an optional pointer-driven guest cursor, tested on Steam Deck.
- Windowed game-frame screenshots and exact fullscreen-presentation captures.

## SimCity Wide Screen

Wide Screen is a game-specific core feature, not frontend stretching. The core
can render 398x239: the original 256-pixel view plus 71 real city-map pixels on
each side. It extends BG1/BG2 sampling, camera limits, the map cursor and
building placement while keeping the HUD and toolbar anchored correctly.
Title, setup, tutorial prompts, scenario selection and modal screens retain
their native layout. The toolbar control is enabled by default, persists in
`settings.ini`, and can be changed while a game is loaded.

## Windows frontend and audio

Version 1.4.0 ports the current Jungle Strike frontend architecture while
retaining and adapting SimCity's widescreen controls and dynamic geometry.
Win32/GDI owns windows and presentation. SDL3 is used only for gamepad access.

The static core produces 32,040 Hz PCM from guest time. The host drains PCM
during long guest frames, applies the selected nearest, linear or cubic-Hermite
speaker resampler, and writes a DirectSound ring buffer. The playback design is
based on the proven Mesen approach: cursor-derived latency measurement,
pre-roll, bounded drift correction, recovery and exact rational frame pacing.
The default speaker rate is 48,000 Hz. Extra forced latency is disabled by
default and, when enabled, ranges from 0 to 40 ms. Host output changes never
alter the core's native PCM authority.

Mesen was used as the hardware-behaviour and frontend-timing reference during
development. Snes9x-derived S-SMP execution semantics remain under their
original license and are restricted to the generated fail-closed SimCity AOT
authority. Neither emulator is embedded as a runtime fallback.

## Linux frontend

The Linux frontend uses SDL2 for window management, rendering and audio. It
accepts the ROM path via command-line argument or the `SIMCITY_ROM_PATH`
environment variable. It is tested on Steam Deck (SteamOS, x86_64), where it
holds the native 60.0988 Hz frame cadence at 1280x800 with widescreen on.

```bash
./simcity-linux --rom /path/to/SimCity\ \(USA\).sfc
```

### Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--rom <path>` | file | required | Path to SimCity (USA).sfc ROM |
| `--resolution <N>` | 0-3 | 0 (480p) | 0=480p, 1=720p, 2=800p, 3=1080p |
| `--size <WxH>` | e.g. `1280x800` | — | Arbitrary window size; overrides `--resolution` |
| `--widescreen` | flag | off | Enable 398x239 core widescreen output |
| `--soft-mouse` | flag | off | Drive the guest d-pad cursor from the host pointer |
| `--mouse-sens <N>` | 1-64 | 2 | Soft-mouse threshold, texture px per frame |

### Settings menu

Press **F1** in game to open the settings overlay. It is drawn on top of the
game frame with a built-in 5x7 bitmap font, so the frontend needs no font
library and no TTF file — only SDL2. Navigate with the arrow keys or the d-pad
(including on Steam Deck): Left/Right changes a value, **B** activates.

The menu **pauses the guest**: no frame advances and no audio drains while it
is open. Advancing would spend money or scroll the city underneath the settings
being changed.

| Row | Effect |
|-----|--------|
| Resolution | 480p / 720p / 800p / 1080p, applied to the live window |
| Widescreen | Toggles the 398x239 core output |
| Soft mouse | Toggles pointer-driven cursor |
| Mouse sens | 1-16, lower is more sensitive |
| Money | One-shot: sets funds to 999999 |
| Freeze money | Toggles topping funds up every frame |
| Resume / Quit | — |

Settings persist to `settings.ini` on exit and are read at startup. The file
uses the same `[General]` section and key names the Windows launcher writes, so
one file works on both frontends. Command-line options override the file.

### Cheats

Only cheat addresses verified against **this** core are offered. Funds sit at
WRAM `$0B9D` as a 24-bit little-endian value; this was confirmed by reaching a
live city and observing `$004E20`, the documented $20000 starting balance.
Addresses used by other emulators for instant-build and disaster toggles were
deliberately left out: they are unverified here and would silently do nothing.

### Pointer input

`--soft-mouse` maps the host pointer onto the guest's own cursor by synthesising
d-pad presses from per-frame pointer motion. The OS cursor is hidden while it is
on, because the guest draws its own.

This is deliberately *not* a SNES Mouse emulation. That peripheral would have to
be answered on a controller port, and the SimCity ROM never asks for one: a
mouse is distinguished from a pad by reads past bit 15 (`$4219`/`$421A`/`$421B`),
and the game reads `$4218` only — two ordinary 2-byte controller reads. Driving
the guest's d-pad cursor is the only mouse-shaped behaviour this title can use.

Motion is mapped by rate, not by position feedback. The core exposes cursor X
(`$025D`, `$01EB`) but has no cursor Y, and guessing that address would steer the
cursor diagonally into map edges. Rate mapping needs no guest state, at the cost
of no self-correction: if the guest's step size differs from pointer speed the
cursor lags slightly. Keyboard and gamepad d-pad always take priority, so the two
input paths cannot fight over the cursor.

Keyboard controls:

| Key | Action |
|-----|--------|
| Arrow keys | D-pad |
| Z | B |
| X | A |
| A | L |
| S | R |
| Return | Start |
| Backspace | Select |
| Q | Y |
| Escape | Close |

On systems where the renderer cannot access `/dev/dri/` directly (headless or
unprivileged environments), force software rendering:

```bash
SDL_VIDEO_RENDERER=software ./simcity-linux --rom /path/to/rom.sfc
```

### Linux build dependencies

| Package | Ubuntu/Debian | Fedora | Arch | openSUSE |
|---------|---------------|--------|------|----------|
| CMake >= 3.20 | `cmake` | `cmake` | `cmake` | `cmake` |
| GCC / Clang | `gcc g++` | `gcc gcc-c++` | `gcc` | `gcc gcc-c++` |
| SDL2 devel | `libsdl2-dev` | `SDL2-devel` | `sdl2` | `SDL2-devel` |

`libsdl2-dev` is required, not optional: the Linux frontend is written against
the SDL2 API and has no SDL3 or Vulkan path. Without it, CMake stops at
configure time and names the package to install. To build only the portable
core and the headless test, skip the frontend:

```sh
cmake -S . -B build -DBUILD_LINUX_FRONTEND=OFF
```

A ROM matching `ROM-REQUIREMENTS.txt` is needed at runtime but is **never**
distributed with the build.

### Steam Deck

The static-recomp core compiles natively on the Steam Deck (x86_64, SteamOS
holo). SDL2 is pre-installed on SteamOS.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run with:

```bash
./build/frontend/linux/simcity-linux --rom /path/to/SimCity\ \(USA\).sfc --resolution 2 --widescreen
```

#### Steam Deck Gaming Mode (Recommended)

For the best experience, use the provided launcher:

```bash
# Copy ROM to data directory
mkdir -p ~/simcity-data
cp /path/to/SimCity\ \(USA\).sfc ~/simcity-data/SimCity.sfc

# Run from Gaming Mode application launcher
# "SimCity SNES Static Recomp" appears in Games category
```

Or via script:

```bash
~/simcity-data/run_simcity.sh
```

The launcher configures:
- Resolution: 800p (1280x800) — optimal for Deck's 1280x800 display
- Widescreen: Enabled (398x239 core output)
- Video driver: Wayland (via gamescope)

#### Headless smoke test (via SSH)

```bash
SDL_VIDEODRIVER=dummy ./build/frontend/linux/simcity-linux --rom "/path/to/SimCity (USA).sfc" --resolution 0
# Gamepad connected: Steam Deck Controller
```

Or use the headless test binary (no window, no audio):

```bash
./build/frontend/linux/headless-test "/path/to/SimCity (USA).sfc" 300
# OK: 300 frames advanced successfully
# Current frame: 300
# Instructions: 3798805
# Master clock: 107159420
```

#### Vulkan

Not implemented. SDL2 is used on all platforms. SDL3 was evaluated and
rejected for now: its 2D renderer and audio APIs differ from SDL2's, so a
correct port is a real port rather than a build-flag change. SDL3 also still
carries the personal-use-only Snes9x terms noted under Licensing, so adding it
is a decision that should be made deliberately rather than by accident.

#### Known Steam Deck Issues

| Issue | Status | Workaround |
|-------|--------|------------|
| Video driver only works in Gaming Mode | Expected | Run from Gaming Mode, not Desktop/SSH |
| SDL2 only; no Vulkan path | Known gap | SDL3's 2D renderer differs from SDL2's API and is not implemented here |
| SSH/X11/Wayland fail outside gamescope | Expected | Use `SDL_VIDEODRIVER=dummy` for headless test |

### Deploy script

`deploy-linux.sh` is the single supported convenience path. It always
configures `Release`, builds, and stages `dist/linux/` with the binary and a
`run.sh` that forwards any extra options:

```bash
./deploy-linux.sh                 # build Release, stage dist/linux/
./deploy-linux.sh --deck HOST     # additionally scp to HOST:~/simcity-build/
```

`run.sh` passes options straight through:

```bash
dist/linux/run.sh rom.sfc --size 1280x800 --widescreen --soft-mouse
```

Plain `cmake` remains the source of truth if you would rather drive the build
yourself.

## Performance notes (measured)

Frame budget on the Steam Deck is 16.641 ms (60.0988 Hz NTSC). Measured with
`SIMCITY_FPS=1`, which reports observed throughput:

| Configuration | Per frame | Observed |
|---------------|-----------|----------|
| `-O0` build, frame rendered twice | ~105 ms | ~10 fps |
| `-O3` Release, frame rendered twice | ~34 ms | ~29 fps |
| `-O3` Release, single render (current) | ~15.4 ms | **57.4 fps avg** |

Frame work is 15.4 ms inside a 16.641 ms budget, leaving ~1.2 ms of slack.
`SDL_Delay` has roughly millisecond granularity and tends to overshoot, so the
loop reaches 60 fps intermittently rather than holding it; the average on the
Deck is 57.4 fps. Reaching a locked 60 would need a sub-millisecond wait tail
(`SDL_DelayNS` or a spin), which was not judged worth the CPU.

Three separate problems were compounding:

1. **Unoptimised build.** With no `CMAKE_BUILD_TYPE` the core compiled at
   `-O0`. The build system now defaults `CMAKE_BUILD_TYPE` to `Release`;
   explicitly select it for any performance measurement.
2. **Every frame was rendered twice.** `simcity_recomp_advance()` performs the
   host frame conversion itself, and the frontend then called
   `simcity_recomp_render_current_frame()` on top of it. The frontend now
   advances with `simcity_recomp_advance_headless()` and converts exactly once.
   The two routes were verified to produce byte-identical framebuffers over 600
   frames in both display modes.
3. **Audio backlog, not underrun.** The SDL device buffer was 8192 frames,
   which is 256 ms of slack at 32,040 Hz. SDL drains PCM in real time, so a
   quarter-second backlog is permanent, audible latency. The device buffer is
   now 512 frames (16 ms), PCM is handed over with `SDL_QueueAudio`, and surplus
   is discarded per frame to bound drift.

   Measured end-to-end queue occupancy on the Deck over 58 samples: **min
   16.6 ms, median 22.1 ms, max 33.3 ms**, with the core-side backlog at zero
   throughout. That is the buffering actually in front of the speaker; the
   512-frame figure is only the device buffer, not the whole latency.

Audio reaches the device with `SDL_QueueAudio` from the main thread. The
callback form let SDL's audio thread read the core's PCM ring while the main
thread was still writing it in `audio_sink()`, which was a data race; queueing
keeps one writer and one reader on different objects without holding a lock
across the whole frame.

Per-frame SHA-256 of the framebuffer is diagnostic metadata with no consumer
outside the renderer, and cost ~3 ms per frame. It is now computed only while a
static-core log is open.

## Licensing

**This repository currently ships no `LICENSE` file, and none can be asserted
here.** This section records the facts rather than granting terms.

- The project is derived from an upstream repository (`Junior-Jones/SimCity-SNES-Static-Recomp`,
  reachable via the `upstream` git remote). Its terms govern what this
  distribution may do; they have not been verified here.
- `static-recomp/static-audio/snes9x-bapu-aot/` contains retained Snes9x
  SPC700 execution semantics. The bundled `SNES9X-LICENSE.txt` is **not** an
  OSI-approved licence. It states Snes9x is "freeware for PERSONAL USE only"
  and that commercial users "should seek permission of the copyright holders
  first".
- Because of that embedded restriction, this combined work cannot be
  relicensed under a permissive licence such as MIT, BSD or Apache by the
  project alone, and commercial redistribution is not clearly permitted.
- `SDL3` (zlib) and the project-owned S-DSP are separable and are documented in
  `THIRD-PARTY-NOTICES.txt`.

Anyone wishing to publish or redistribute this must first resolve the Snes9x
personal-use-only clause with its copyright holders, and confirm the upstream
terms. That is a legal decision for the copyright holder, not a build setting.

## Cheats and mods (host-side WRAM access)

`simcity_recomp_write_wram()` lets a frontend poke guest WRAM between frames,
which is how cheats and mods are implemented. It is the write counterpart to the
existing bounds-checked `simcity_recomp_read_wram()`.

```c
uint8_t money[3] = { 0x3F, 0x42, 0x0F };   /* 999999 */
simcity_recomp_write_wram(game, 0x0B9D, money, 3);
```

Contract, and the parts that are easy to get wrong:

| Rule | Reason |
|------|--------|
| Call from the thread that calls `simcity_recomp_advance()` | the core cannot observe a concurrent host write |
| Call between frames, never during one | a write mid-advance is a data race |
| Nothing is written when the call returns 0 | a rejected write must not leave WRAM half-modified |
| Range must lie inside the 128 KiB image | offsets at or past `0x20000` are rejected |

The write is visible to the next frame as if the guest had stored those bytes.
Battery SRAM, S-SMP and S-DSP state are untouched, and later snapshots capture
it because WRAM is part of the saved runtime image.

**Do not poke PPU or camera state.** The core caches widescreen cursor anchors
in the instance rather than in WRAM, so writing `$01BD`, `$0139`, `$01EB` or
`$025D` behind its back desynchronises that cache. Cheats should target game
state such as funds at `$0B9D`.

`static-recomp/tests/test_wram_write.c` pins the bounds contract, the
no-partial-write guarantee, the round trip, and that the static route survives a
poke. It runs under `ctest` when `-DSIMCITY_TEST_ROM=<rom>` is configured.

## Known issues

Tracked as tickets under `aes/tickets/` following a four-persona adversarial
review. The ones that affect a user today:

| Ticket | Effect |
|--------|--------|
| T004 | On a host without system SDL2, CMake reports success and the build then fails. |
| T005 | Audio back-pressure drops all queued PCM rather than the surplus, causing audible gaps. |
| T006 | The settings menu cannot be opened on Steam Deck without a keyboard. |
| T008 | Quitting discards the saved city: battery SRAM is never written on Linux. |
| T002 | README (earlier revision) pointed at a launcher script that is not in the repository. |
| T007 | No CI job builds the Linux frontend, which is how T004 shipped. |

Fixed in the review round: the red/blue channel swap (B1), `--resolution` being
discarded (B2), the inert `FREEZE MONEY` row (B3), `settings.ini` deleting every
Windows-launcher key (B4), a frozen pointer delta steering the menu (B5), divergent
`MouseSens` bounds (T009), and a README code fence that swallowed three sections
(T010).

## Verification

The source includes contract tests for controller ordering, configuration,
resampling, audio defaults, project-owned DSP phase timing, linked DSP purity
and real-ROM snapshot PCM continuation. Long headless routes cover neutral
execution, tutorial gameplay, HUD transitions, toolbar focus, map edges,
wide-cursor travel, construction, scenarios, Freeland and all four map corners.
Unknown static CPU/audio states and PCM overflow remain release failures.

Third-party notices and licenses are in `THIRD-PARTY-NOTICES.txt` and the
corresponding source directories.
