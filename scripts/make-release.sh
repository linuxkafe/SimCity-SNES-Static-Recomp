#!/bin/sh
#
# make-release.sh — build the binary that ships to users, and prove it carries
# no game data.
#
# The public installer downloads this instead of compiling on the Deck, so the
# thing being published has to be checked, not assumed.  A prebuilt binary is
# only acceptable as long as it contains nothing from the ROM, and the strongest
# evidence available is the ROM itself: if any 4 KiB of it appears inside the
# binary, the binary embeds game data and this script fails.
#
# Usage: scripts/make-release.sh [rom.sfc]
#
# Writes to dist/linux/:
#   simcity-linux        stripped executable
#   simcity-linux.sha256 checksum
#   release.json         version, build info and the checks that were run

set -eu

ROM_ARG="${1:-}"
ROOT="$(cd -- "$(dirname -- "$0")/.." 2>/dev/null && pwd -P)"
OUT="$ROOT/dist/linux"

say()  { printf '\n\033[1;36m==>\033[0m %s\n' "$*"; }
info() { printf '    %s\n' "$*"; }
die()  { printf '\033[1;31merro:\033[0m %s\n' "$*" >&2; exit 1; }

for tool in cmake make c++ strip sha256sum python3; do
    command -v "$tool" >/dev/null 2>&1 || die "falta a ferramenta $tool"
done

# ---------------------------------------------------------------------------
say "A compilar"

BUILD="$ROOT/build-release"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$BUILD" -j "$(nproc)" > /dev/null

BIN="$BUILD/frontend/linux/simcity-linux"
[ -x "$BIN" ] || die "a compilacao nao produziu $BIN"

mkdir -p "$OUT"
strip --strip-unneeded -o "$OUT/simcity-linux" "$BIN"
chmod +x "$OUT/simcity-linux"
info "$(du -h "$OUT/simcity-linux" | cut -f1)  $OUT/simcity-linux"

# ---------------------------------------------------------------------------
say "A verificar que o binario nao contem dados da ROM"

python3 "$ROOT/scripts/verify-release.py" "$OUT/simcity-linux" "$ROM_ARG" \
    || die "o binario nao passou a verificacao; nao vao publicar"

# ---------------------------------------------------------------------------
say "A escrever o manifesto"

( cd "$OUT" && sha256sum simcity-linux > simcity-linux.sha256 )
info "$(cut -d' ' -f1 < "$OUT/simcity-linux.sha256")  simcity-linux"

VERSION="unknown"
[ -f "$ROOT/VERSION.txt" ] && VERSION="$(tr -d ' \t\n\r' < "$ROOT/VERSION.txt")"
COMMIT="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
BUILT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

cat > "$OUT/release.json" <<EOF
{
  "version": "$VERSION",
  "commit": "$COMMIT",
  "built_utc": "$BUILT",
  "binary": "simcity-linux",
  "sha256": "$(cut -d' ' -f1 < "$OUT/simcity-linux.sha256")",
  "size_bytes": $(wc -c < "$OUT/simcity-linux"),
  "architecture": "$(uname -m)",
  "requires_rom": true,
  "rom_bytes": 524288,
  "rom_shipped": false,
  "rom_asset_check": "no 4 KiB window of the ROM is present in the binary"
}
EOF

say "Pronto"
printf '\n'
printf '    binario : %s\n' "$OUT/simcity-linux"
printf '    sha256  : %s\n' "$OUT/simcity-linux.sha256"
printf '    manifesto: %s\n' "$OUT/release.json"
printf '\n'