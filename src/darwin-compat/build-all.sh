#!/bin/bash
# Build all CatBSD Darwin utilities

set -e

echo "Building CatBSD Darwin Utilities..."
echo ""

# Build shim library first
echo "=== Building compatibility shim library ==="
cd shims
make clean
make
echo "✓ Shim library built"
echo ""

# Build utilities
UTILITIES="sw_vers-demo plutil-demo say-demo caffeinate-demo launchd-demo"

for util in $UTILITIES; do
    echo "=== Building $util ==="
    cd ../$util
    make clean
    make
    echo "✓ $util built"
    echo ""
done

cd ..

echo "╔════════════════════════════════════════╗"
echo "║   ✅ All utilities built successfully!  ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Run ./demo.sh to see them in action!"
