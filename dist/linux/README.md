# SimCity SNES Static Recomp - Linux

## Run
```bash
./run.sh /path/to/SimCity\ \(USA\).sfc
```

Or set ROM path:
```bash
export SIMCITY_ROM_PATH=/path/to/rom.sfc
./simcity-linux --resolution 2 --widescreen
```

## Options
- --resolution 0=480p, 1=720p, 2=800p, 3=1080p
- --widescreen Enable widescreen mode
- --rom Path to ROM file

## Requirements
- SDL2 2.0+
- Linux x86_64
