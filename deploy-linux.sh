#!/bin/bash
# SimCity Linux Deploy Script - Simple binary deployment

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build/frontend/linux"
ROM_DIR="$PROJECT_ROOT/Rom"
OUTPUT_DIR="$PROJECT_ROOT/dist/linux"

echo "=== SimCity Linux Deploy ==="

# Build core if needed
if [ ! -f "$BUILD_DIR/simcity-linux" ]; then
    echo "[1/3] Building Linux frontend..."
    cd "$PROJECT_ROOT/build"
    cmake .. -DBUILD_LINUX_FRONTEND=ON
    make simcity-linux -j4
fi

# Create dist directory
mkdir -p "$OUTPUT_DIR"

# Copy binary
echo "[2/3] Copying binary to dist..."
cp "$BUILD_DIR/simcity-linux" "$OUTPUT_DIR/"

# Create simple run script
cat > "$OUTPUT_DIR/run.sh" << 'RUNEOF'
#!/bin/bash
ROM_PATH="${1:-$SIMCITY_ROM_PATH}"
if [ -z "$ROM_PATH" ]; then
    echo "Usage: $0 <rom_path>"
    echo "Or set SIMCITY_ROM_PATH environment variable"
    exit 1
fi
./simcity-linux --rom "$ROM_PATH" --resolution 2 --widescreen
RUNEOF

chmod +x "$OUTPUT_DIR/run.sh"

# Create README
cat > "$OUTPUT_DIR/README.md" << 'READEOF'
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
READEOF

echo "[3/3] Deploy complete: $OUTPUT_DIR"
ls -lh "$OUTPUT_DIR/simcity-linux"
echo ""
echo "Run with: $OUTPUT_DIR/run.sh <rom_path>"
