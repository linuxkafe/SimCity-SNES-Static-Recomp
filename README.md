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

## Building on Windows

Requirements:

- Windows 10 or later
- CMake 3.20 or later
- Visual Studio 2022 with Desktop development with C++
- Internet access during the first configure for pinned SDL3 gamepad source

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`build\Release\Launcher.exe` is the portable application. The optional
`SIMCITY_TEST_ROM` CMake path enables the local real-ROM audio snapshot
continuation test; the ROM is never copied into source or release artifacts.

## Building on Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

The executable is at `build/frontend/linux/simcity-linux`. A headless test
binary (no window, no audio) is built at `build/frontend/linux/headless-test`
and can be used for CI or on machines without a display server:

```bash
./scripts/headless-test.sh /path/to/SimCity\ \(USA\).sfc 300
# OK: 300 frames advanced successfully
# Current frame: 300
# Instructions: 3798805
# Master clock: 107159420
```

To build the core only (no frontend) on a system lacking SDL2:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_LINUX_FRONTEND=OFF
cmake --build build -j$(nproc)
```

Running `ctest` without a ROM skips the real-ROM audio snapshot test; all
pure-unit tests still run.

### Steam Deck

The static-recomp core compiles natively on the Steam Deck (ARM64, SteamOS
holo). Install SDL2 first:

```bash
sudo pacman -S sdl2
```

Then build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run with:

```bash
./build/frontend/linux/simcity-linux --rom /path/to/SimCity\ \(USA\).sfc
```

For a headless smoke test on the Deck (e.g. via SSH):

```bash
./build/frontend/linux/headless-test "/path/to/SimCity (USA).sfc" 300
```

The Deck has `/dev/dri` access by default when logged in as `deck`; if
running under `sudo` or in a container, grant access first:

```bash
sudo chmod 666 /dev/dri/renderD128 /dev/dri/card0
```

## Verification

The source includes contract tests for controller ordering, configuration,
resampling, audio defaults, project-owned DSP phase timing, linked DSP purity
and real-ROM snapshot PCM continuation. Long headless routes cover neutral
execution, tutorial gameplay, HUD transitions, toolbar focus, map edges,
wide-cursor travel, construction, scenarios, Freeland and all four map corners.
Unknown static CPU/audio states and PCM overflow remain release failures.

Third-party notices and licenses are in `THIRD-PARTY-NOTICES.txt` and the
corresponding source directories.
