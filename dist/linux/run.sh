#!/bin/bash
ROM_PATH="${1:-$SIMCITY_ROM_PATH}"
if [ -z "$ROM_PATH" ]; then
    echo "Usage: $0 <rom_path>"
    echo "Or set SIMCITY_ROM_PATH environment variable"
    exit 1
fi
./simcity-linux --rom "$ROM_PATH" --resolution 2 --widescreen
