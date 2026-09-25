#!/bin/bash
# Build all CatBSD Darwin utilities (Essential 15 + shim library)
# Usage: ./build-all.sh [clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Support libraries, built before the utilities that link them.
# blocks must come first: it is a dependency for the upstream libdispatch
# import. shims comes second; nothing else depends on the other two yet.
LIBRARIES="blocks shims libdispatch launchd"

# All 15 demo utilities
UTILITIES="sw_vers-demo plutil-demo say-demo caffeinate-demo launchd-demo \
           afplay-demo ditto-demo defaults-demo networksetup-demo open-demo \
           pbcopy-demo pbpaste-demo screencapture-demo scutil-demo xattr-demo"

# Handle optional 'clean' argument
if [ "${1:-}" = "clean" ]; then
    echo "Cleaning CatBSD Darwin Utilities..."
    echo ""
    for lib in $LIBRARIES; do
        (cd "$lib" && make clean)
    done
    for util in $UTILITIES; do
        (cd "$util" && make clean)
    done
    echo "✓ All clean"
    exit 0
fi

# Handle optional 'test' argument: run every library's test suite.
if [ "${1:-}" = "test" ]; then
    echo "Running CatBSD library test suites..."
    echo ""
    for lib in $LIBRARIES; do
        echo "=== $lib ==="
        (cd "$lib" && make test)
        echo ""
    done
    echo "✓ All library tests passed"
    exit 0
fi

echo "Building CatBSD Darwin Utilities (Essential 15)..."
echo ""

# Build support libraries first (shims is a dependency for launchd-demo and
# caffeinate-demo; the others stand alone for now)
for lib in $LIBRARIES; do
    echo "=== Building $lib ==="
    (cd "$lib" && make)
    echo "✓ $lib built"
    echo ""
done
cd "$SCRIPT_DIR"

# Track results
BUILT=0
FAILED=""

for util in $UTILITIES; do
    echo "=== Building $util ==="
    if (cd "$util" && make 2>&1); then
        echo "✓ $util built"
        BUILT=$((BUILT + 1))
    else
        echo "✗ $util FAILED"
        FAILED="$FAILED $util"
    fi
    echo ""
done

echo "╔════════════════════════════════════════════╗"
if [ -z "$FAILED" ]; then
    printf "║  ✅  All %d utilities built successfully!   ║\n" "$BUILT"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    echo "Run ./demo-essential-15.sh to see all 15 in action."
    echo "Run ./demo.sh for the original 5-utility demo."
else
    printf "║  ⚠️   %d built,  failed:%s\n" "$BUILT" "$FAILED"
    echo "╚════════════════════════════════════════════╝"
    exit 1
fi
