#!/bin/sh
# test-freebsd.sh — Run the full CatBSD test suite on a FreeBSD host
#
# Expected to be run from inside a FreeBSD VM (or natively).
# All components use only libc + pthreads + kqueue — no pkg deps required.
#
# Usage:
#   ./scripts/test-freebsd.sh           # run everything
#   ./scripts/test-freebsd.sh libs      # library test suites only
#   ./scripts/test-freebsd.sh init      # catbsd-init smoke tests only
#   ./scripts/test-freebsd.sh build     # build only, no tests
#
# Exit codes:
#   0  all tests passed
#   1  one or more failures (error printed to stderr)

set -e

BASE="$(cd "$(dirname "$0")/.." && pwd)"
COMPAT="$BASE/src/darwin-compat"
INIT="$BASE/src/catbsd-init"
MODE="${1:-all}"

PASS=0
FAIL=0
ERRORS=""

# ── Colour output ────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    GREEN='\033[0;32m'; RED='\033[0;31m'; BOLD='\033[1m'; RESET='\033[0m'
else
    GREEN=''; RED=''; BOLD=''; RESET=''
fi

ok()   { printf "${GREEN}  ✓${RESET} %s\n" "$1"; PASS=$((PASS+1)); }
fail() { printf "${RED}  ✗${RESET} %s\n" "$1"; FAIL=$((FAIL+1)); ERRORS="$ERRORS\n  • $1"; }
hdr()  { printf "\n${BOLD}=== %s ===${RESET}\n" "$1"; }

# ── Build helper ─────────────────────────────────────────────────────────────
build_dir() {
    dir="$1"; label="$2"
    printf "  Building %-30s" "$label..."
    if make -C "$dir" -s 2>/tmp/catbsd-build-err; then
        printf "ok\n"
        return 0
    else
        printf "FAILED\n"
        cat /tmp/catbsd-build-err >&2
        fail "build: $label"
        return 1
    fi
}

# ── Test helper ──────────────────────────────────────────────────────────────
run_test() {
    dir="$1"; target="$2"; label="$3"
    if make -C "$dir" "$target" -s >/tmp/catbsd-test-out 2>&1; then
        ok "$label"
    else
        fail "$label"
        cat /tmp/catbsd-test-out >&2
    fi
}

# ── Sanity check ─────────────────────────────────────────────────────────────
hdr "CatBSD FreeBSD Test Suite"
printf "Host:    %s %s\n" "$(uname -s)" "$(uname -r)"
printf "Arch:    %s\n"    "$(uname -m)"
printf "CC:      %s\n"    "$(cc --version 2>&1 | head -1)"
printf "BaseDir: %s\n"    "$BASE"
echo ""

# Confirm we're on FreeBSD (warn but don't abort — macOS testing is fine too)
OS="$(uname -s)"
if [ "$OS" != "FreeBSD" ]; then
    printf "  NOTE: running on %s, not FreeBSD. Results still useful.\n\n" "$OS"
fi

# ── Build phase ───────────────────────────────────────────────────────────────
if [ "$MODE" = "all" ] || [ "$MODE" = "libs" ] || [ "$MODE" = "build" ]; then
    hdr "Building libraries"
    build_dir "$COMPAT/blocks"    "blocks (libBlocksRuntime)"
    build_dir "$COMPAT/shims"     "shims (libdarwin_compat)"
    build_dir "$COMPAT/libdispatch" "libdispatch subset"
    build_dir "$COMPAT/launchd"   "launchd (liblaunch)"
    build_dir "$INIT"             "catbsd-init"
    build_dir "$COMPAT/launchctl-demo" "catbsd-launchctl"
fi

if [ "$MODE" = "build" ]; then
    echo ""
    printf "${GREEN}Build complete.${RESET}\n"
    exit 0
fi

# ── Library test suites ───────────────────────────────────────────────────────
if [ "$MODE" = "all" ] || [ "$MODE" = "libs" ]; then
    hdr "Library test suites"

    run_test "$COMPAT/blocks"       "test" "Blocks Runtime — stack, copy/release, capture, nested"
    run_test "$COMPAT/shims"        "test" "Mach port shim + XPC shim"
    run_test "$COMPAT/libdispatch"  "test" "libdispatch — serial, concurrent, barriers, timers"
    run_test "$COMPAT/launchd"      "test" "liblaunch — plist, supervision, KeepAlive, sockets"
    run_test "$COMPAT/launchd"      "test-launchctl" "launchctl protocol — all 6 verbs over XPC"
fi

# ── catbsd-init smoke tests ───────────────────────────────────────────────────
if [ "$MODE" = "all" ] || [ "$MODE" = "init" ]; then
    hdr "catbsd-init smoke tests"
    run_test "$INIT" "test" "catbsd-init — empty-dir, single-user, daemon-load"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
hdr "Results"
printf "  Passed: ${GREEN}%d${RESET}\n" "$PASS"
if [ "$FAIL" -gt 0 ]; then
    printf "  Failed: ${RED}%d${RESET}\n" "$FAIL"
    printf "\nFailed tests:%b\n" "$ERRORS"
    echo ""
    printf "${RED}FAILED${RESET} — %d test suite(s) did not pass.\n" "$FAIL"
    exit 1
else
    printf "\n${GREEN}${BOLD}All %d test suites passed on %s %s!${RESET}\n" \
           "$PASS" "$OS" "$(uname -r)"
fi
