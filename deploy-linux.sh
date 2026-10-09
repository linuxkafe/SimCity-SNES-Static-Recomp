#!/bin/bash
# SimCity SNES Static Recomp — Linux build and deploy.
#
#   ./deploy-linux.sh              build Release and stage dist/linux/
#   ./deploy-linux.sh --deck HOST  additionally scp the binary to HOST:~/simcity-build/
#
# Always configures and builds: shipping a stale binary because one already
# existed is worse than a few seconds of extra compile time.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
STAGE_DIR="$PROJECT_ROOT/dist/linux"
DECK_HOST=""

if [ "${1:-}" = "--deck" ]; then
    DECK_HOST="${2:-deck@steamdeck}"
elif [ $# -gt 0 ]; then
    echo "Usage: $0 [--deck HOST]" >&2
    exit 1
fi

# Release matters: an unoptimised core runs roughly three times slower and
# misses the 16.6 ms frame budget on its own.
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_LINUX_FRONTEND=ON
cmake --build "$BUILD_DIR" --target simcity-linux -j"$(nproc)"

mkdir -p "$STAGE_DIR"
cp "$BUILD_DIR/frontend/linux/simcity-linux" "$STAGE_DIR/"

cat > "$STAGE_DIR/run.sh" << 'RUNEOF'
#!/bin/bash
# Forwards every argument, so options like --soft-mouse or --size pass through.
set -e
ROM_PATH="${1:-${SIMCITY_ROM_PATH:-}}"
if [ -z "$ROM_PATH" ] || [ ! -f "$ROM_PATH" ]; then
    echo "Usage: $0 <rom.sfc> [options]" >&2
    echo "Or set SIMCITY_ROM_PATH." >&2
    exit 1
fi
shift
cd "$(dirname "$0")"
exec ./simcity-linux --rom "$ROM_PATH" "$@"
RUNEOF
chmod +x "$STAGE_DIR/run.sh"

echo
echo "Staged: $STAGE_DIR/simcity-linux"
"$STAGE_DIR/simcity-linux" --help 2>&1 | head -1

if [ -n "$DECK_HOST" ]; then
    ssh "$DECK_HOST" "mkdir -p ~/simcity-build"
    scp "$STAGE_DIR/simcity-linux" "$DECK_HOST:~/simcity-build/simcity-linux"
    echo "Deployed to $DECK_HOST:~/simcity-build/simcity-linux"
fi