#!/usr/bin/env bash
# T071 – dirty-tile render path identity gate
set -euo pipefail
ROM="${1:?usage: $0 <rom> [frames]}"
FRAMES="${2:-700}"
SPEC="10:B,400:0,420:RIGHT,520:0,540:LEFT,640:0"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HP="$ROOT/build/frontend/linux/headless-profile"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/OFF/shots" "$TMP/ON/shots"

echo "Run dirty=OFF"
"$HP" "$ROM" "$FRAMES" --input "$SPEC" --hash --no-pc-profile \
     --shots "$TMP/OFF/shots" \
     --wram-dump "$((FRAMES-1)):$TMP/OFF/wram.bin" >"$TMP/OFF.out" 2>&1

echo "Run dirty=ON"
"$HP" "$ROM" "$FRAMES" --input "$SPEC" --hash --no-pc-profile \
     --shots "$TMP/ON/shots" \
     --wram-dump "$((FRAMES-1)):$TMP/ON/wram.bin" >"$TMP/ON.out" 2>&1

fail=0
check(){ local m="$1"; shift; if "$@"; then echo "  PASS $m"; else echo "  FAIL $m"; fail=1; fi; }

check "master clock"  diff -q <(grep '^Master clock:' "$TMP/OFF.out") <(grep '^Master clock:' "$TMP/ON.out")
check "instructions"  diff -q <(grep '^Instructions:' "$TMP/OFF.out") <(grep '^Instructions:' "$TMP/ON.out")
check "hash stream"   diff -q <(grep '^screen: frame ' "$TMP/OFF.out") <(grep '^screen: frame ' "$TMP/ON.out")
check "shots"         diff -r "$TMP/OFF/shots" "$TMP/ON/shots"
check "WRAM"          cmp -s "$TMP/OFF/wram.bin" "$TMP/ON/wram.bin"

[[ $fail -eq 0 ]] && echo "GATE: PASS" || { echo "GATE: FAIL"; exit 1; }