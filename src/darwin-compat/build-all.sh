#!/bin/bash
# Build all CatBSD Darwin utilities (Essential 15 + shim library)
# Usage: ./build-all.sh [clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# All 15 demo utilities
UTILITIES="sw_vers-demo plutil-demo say-demo caffeinate-demo launchd-demo \
           afplay-demo ditto-demo defaults-demo networksetup-demo open-demo \
           pbcopy-demo pbpaste-demo screencapture-demo scutil-demo xattr-demo"

# Handle optional 'clean' argument
if [ "${1:-}" = "clean" ]; then
    echo "Cleaning CatBSD Darwin Utilities..."
    echo ""
    (cd shims && make clean)
    for util in $UTILITIES; do
        (cd "$util" && make clean)
    done
    echo "✓ All clean"
    exit 0
fi

echo "Building CatBSD Darwin Utilities (Essential 15)..."
echo ""

# Build shim library first (dependency for launchd-demo and caffeinate-demo)
echo "=== Building compatibility shim library ==="
cd shims
make
echo "✓ Shim library built"
echo ""
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
