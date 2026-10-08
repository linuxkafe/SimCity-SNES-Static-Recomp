#!/bin/bash
# headless-test.sh — advance N frames of the static-recomp core without
# requiring a display server or audio hardware.
#
# Usage:
#   ./scripts/headless-test.sh <rom-path> [frames]
#
# The optional second argument defaults to 300 frames.
# Exit 0 means the core advanced without route failure; exit 1 on error.
# The binary is built at build/frontend/linux/headless-test.

set -euo pipefail

ROM_PATH="${1:?Usage: $0 <rom-path> [frames]}"
FRAMES="${2:-300}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${REPO_ROOT}/build/frontend/linux/headless-test"

if [[ ! -x "$BIN" ]]; then
    echo "ERROR: headless-test not found at ${BIN}" >&2
    echo "Build first:" >&2
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release" >&2
    echo "  cmake --build build -j$(nproc)" >&2
    exit 1
fi

if [[ ! -f "$ROM_PATH" ]]; then
    echo "ERROR: ROM not found at ${ROM_PATH}" >&2
    exit 1
fi

exec "$BIN" "$ROM_PATH" "$FRAMES"
