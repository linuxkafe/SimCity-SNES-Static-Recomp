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
  frontend (SDL2, windowed).
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
environment variable.

```bash
./simcity-linux --rom /path/to/SimCity\ \(USA\).sfc
```

### Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `--rom <path>` | file | required | Path to SimCity (USA).sfc ROM |
| `--resolution <N>` | 0-3 | 0 (480p) | 0=480p, 1=720p, 2=800p, 3=1080p |
| `--widescreen` | flag | off | Enable 398x239 core widescreen output |

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

#### SDL3/Vulkan Build (Experimental)

For native Vulkan renderer on Steam Deck:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSDL_FORCE_SDL3=ON
cmake --build build -j$(nproc)
```

This fetches and builds SDL3 from source with Vulkan support enabled.

#### Known Steam Deck Issues

| Issue | Status | Workaround |
|-------|--------|------------|
| Video driver only works in Gaming Mode | Expected | Run from Gaming Mode, not Desktop/SSH |
| SDL2 used by default (not Vulkan) | By design | Use `-DSDL_FORCE_SDL3=ON` for Vulkan |
| SSH/X11/Wayland fail outside gamescope | Expected | Use `SDL_VIDEODRIVER=dummy` for headless test |

### Quick Deploy Script

```bash
./deploy-linux.sh
# Creates dist/linux/ with binary, run script, and README
```

Or use the simplified Makefile:

```bash
make -f Makefile.linux.simple dist       # Build + create dist/
make -f Makefile.linux.simple steam-deck # Build + deploy to Steam Deck via scp
```

## Performance notes (measured)

Frame budget on the Steam Deck is 16.641 ms (60.0988 Hz NTSC). Measured with
`SIMCITY_FPS=1`, which reports observed throughput:

| Configuration | Per frame | Observed |
|---------------|-----------|----------|
| `-O0` build, frame rendered twice | ~105 ms | ~10 fps |
| `-O3` Release, frame rendered twice | ~34 ms | ~29 fps |
| `-O3` Release, single render (current) | ~15.4 ms | **56-57 fps** |

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
   quarter-second backlog is permanent, audible latency. The buffer is now 512
   frames (~16 ms) and surplus PCM is discarded per frame to bound drift.

Audio reaches the device with `SDL_QueueAudio` from the main thread. The
callback form let SDL's audio thread read the core's PCM ring while the main
thread was still writing it in `audio_sink()`, which was a data race; queueing
keeps one writer and one reader on different objects without holding a lock
across the whole frame.

Per-frame SHA-256 of the framebuffer is diagnostic metadata with no consumer
outside the renderer, and cost ~3 ms per frame. It is now computed only while a
static-core log is open.

## Verification

The source includes contract tests for controller ordering, configuration,
resampling, audio defaults, project-owned DSP phase timing, linked DSP purity
and real-ROM snapshot PCM continuation. Long headless routes cover neutral
execution, tutorial gameplay, HUD transitions, toolbar focus, map edges,
wide-cursor travel, construction, scenarios, Freeland and all four map corners.
Unknown static CPU/audio states and PCM overflow remain release failures.

Third-party notices and licenses are in `THIRD-PARTY-NOTICES.txt` and the
corresponding source directories.
