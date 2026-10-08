SimCity SNES Static Recomp

Place the matching .sfc ROM beside the executable or pass its path with --rom.
The filename can be anything. The ROM is not included.

Linux (windowed):
    ./build/frontend/linux/simcity-linux --rom "/path/to/SimCity (USA).sfc"

Linux (headless smoke test, no display needed):
    ./build/frontend/linux/headless-test "/path/to/SimCity (USA).sfc" 300

Windows:
    Launcher.exe  (use Browse to select the ROM, or place it in Rom\ then choose Run)

Save data is written atomically beside the executable:
- Linux: the core holds state in memory; saves require a host-level snapshot API
- Windows: Saves\SimCity-USA.srm

The Linux frontend uses SDL2 for rendering and audio at 32,040 Hz.
Press Escape to quit. Keyboard: arrows=D-pad, Z=B, X=A, A=L, S=R,
Return=Start, Backspace=Select, Q=Y.

See ROM-REQUIREMENTS.txt for the required ROM details.
