#!/bin/sh
# Verify Implementation — checks acceptance criteria for a ticket
# Usage: scripts/verify-implementation.sh [TICKET_ID]
# If no ticket ID given, reads current_ticket from aes/kanban.md

set -e

case "${1:-}" in
--help | -h)
	echo "Usage: $(basename "$0") [TICKET_ID]"
	echo ""
	echo "Checks acceptance criteria for a ticket."
	echo "If no ticket ID given, reads current_ticket from aes/kanban.md"
	echo ""
	echo "Exit code: 0 if all criteria pass, 1 if any fail"
	exit 0
	;;
esac

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0
COUNT=0

log() { printf "  %s\n" "$*"; }
pass() {
	COUNT=$((COUNT + 1))
	log "✅ $1"
}
fail() {
	COUNT=$((COUNT + 1))
	FAIL=$((FAIL + 1))
	log "❌ $1"
}
skip() {
	COUNT=$((COUNT + 1))
	log "⏭️  $1"
}
bail() {
	log "ERROR: $1"
	exit 1
}

# ── Resolve ticket ID ──────────────────────────────────────────────────────────
TICKET="${1:-}"
if [ -z "$TICKET" ]; then
	if [ -f "$SCRIPT_DIR/aes/kanban.md" ]; then
		TICKET=$(grep "^current_ticket:" "$SCRIPT_DIR/aes/kanban.md" | awk '{print $2}')
	fi
	if [ -z "$TICKET" ]; then
		bail "No ticket ID given and no current_ticket in aes/kanban.md"
	fi
fi

NAME_FILE=$(find "$SCRIPT_DIR/aes/tickets" -name "${TICKET}-*.md" 2>/dev/null | head -1)
if [ ! -f "$NAME_FILE" ]; then
	bail "Ticket file not found for $TICKET in aes/tickets/"
fi

echo ""
echo "══════════════════════════════════════════════════"
echo "  Verification Gate — $TICKET"
echo "  File: $(basename "$NAME_FILE")"
echo "══════════════════════════════════════════════════"
echo ""

# ── Extract acceptance criteria ───────────────────────────────────────────────
# Lines matching "- [ ] " or "- [x] " after "Acceptance Criteria" section
IN_SECTION=0
CRITERIA_FILE=$(mktemp)
trap 'rm -f "$CRITERIA_FILE"' EXIT

while IFS= read -r line; do
	case "$line" in
	*"Acceptance Criteria"*)
		IN_SECTION=1
		continue
		;;
	"##"*) [ "$IN_SECTION" = "1" ] && break ;;
	esac
	if [ "$IN_SECTION" = "1" ]; then
		case "$line" in
		*"- ["*"] "*) printf "%s\n" "$line" >>"$CRITERIA_FILE" ;;
		esac
	fi
done <"$NAME_FILE"

if [ ! -s "$CRITERIA_FILE" ]; then
	skip "No acceptance criteria found in $(basename "$NAME_FILE")"
	echo ""
	echo "────────────────────────────────────────────────────"
	echo "  Result: 0 passed, 0 failed, $COUNT total"
	echo "────────────────────────────────────────────────────"
	exit 0
fi

# ── Verify each criterion ──────────────────────────────────────────────────────
while IFS= read -r line; do
	[ -z "$line" ] && continue
	checked=$(echo "$line" | sed -n 's/.*\[\(.\)\].*/\1/p')
	text=$(echo "$line" | sed 's/.*\] //')

	case "$checked" in
	"x")
		pass "(already checked) $text"
		continue
		;;
	esac

	result=""
	evidence=""

	# Extract backtick-quoted path (file or command)
	file=$(echo "$text" | sed -n 's/.*`\([^`]*\)`.*/\1/p')

	# 1) File existence check
	if echo "$text" | grep -iq "script exists\|file exists\|hook exists\|exists at"; then
		if [ -n "$file" ] && [ -f "$SCRIPT_DIR/$file" ]; then
			result="pass"
			evidence="exists at $file"
		elif [ -n "$file" ]; then
			result="fail"
			evidence="NOT FOUND at $file"
		fi
	fi

	# 2) Executable check
	if [ -z "$result" ] && echo "$text" | grep -iq "executable\|is executable"; then
		if [ -n "$file" ] && [ -x "$SCRIPT_DIR/$file" ]; then
			result="pass"
			evidence="$file is executable"
		elif [ -n "$file" ]; then
			result="fail"
			evidence="$file NOT executable or not found"
		fi
	fi

	# 3) Command outputs usage with --help
	if [ -z "$result" ] && echo "$text" | grep -iq -- "--help\|shows usage"; then
		if [ -n "$file" ]; then
			if bash -c "$file --help" >/dev/null 2>&1; then
				result="pass"
				evidence="'$file --help' exits 0"
			else
				result="fail"
				evidence="'$file --help' FAILED"
			fi
		fi
	fi

	# 4) File content / grep based
	if [ -z "$result" ] && echo "$text" | grep -iq "contains\|prints\|outputs\|shows"; then
		grep_term=$(echo "$text" | sed -n "s/.*['\"]\([^'\"]*\)['\"].*/\1/p")
		if [ -n "$grep_term" ] && [ -n "$file" ]; then
			if grep -q "$grep_term" "$SCRIPT_DIR/$file" 2>/dev/null; then
				result="pass"
				evidence="'$grep_term' found in $file"
			else
				result="fail"
				evidence="'$grep_term' NOT found in $file"
			fi
		fi
	fi

	# 5) Makefile target exists
	if [ -z "$result" ] && echo "$text" | grep -iq "make.*target\|target exists"; then
		target=$(echo "$text" | sed 's/.*make \([a-zA-Z0-9_-]*\).*/\1/')
		if [ -n "$target" ] && grep -q "^${target}:" "$SCRIPT_DIR/Makefile" 2>/dev/null; then
			result="pass"
			evidence="make $target target exists"
		elif [ -n "$target" ]; then
			result="fail"
			evidence="make $target target NOT found"
		fi
	fi

	# 6) Command exits with 0
	if [ -z "$result" ] && echo "$text" | grep -iq "exits 0\|exits with\|passes\|all.*pass"; then
		if [ -n "$file" ]; then
			if bash -c "$file" >/dev/null 2>&1; then
				result="pass"
				evidence="'$file' exits 0"
			else
				result="fail"
				evidence="'$file' FAILED"
			fi
		fi
	fi

	# 7) Fallback: file path mentioned and it exists
	if [ -z "$result" ] && [ -n "$file" ] && [ -f "$SCRIPT_DIR/$file" ]; then
		if echo "$file" | grep -q '[.]'; then
			result="pass"
			evidence="found: $file"
		fi
	fi

	case "$result" in
	pass) pass "$text — $evidence" ;;
	fail) fail "$text — $evidence" ;;
	*) skip "Cannot auto-verify: $text" ;;
	esac
done <"$CRITERIA_FILE"

echo ""
echo "────────────────────────────────────────────────────"
echo "  Result: $((COUNT - FAIL)) passed, $FAIL failed, $COUNT total"
echo "────────────────────────────────────────────────────"

[ "$FAIL" -eq 0 ]
