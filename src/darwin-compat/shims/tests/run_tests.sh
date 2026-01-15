#!/bin/sh
# Build and run shim tests

set -e

echo "Building CatBSD shim library tests..."
echo ""

# Build the shim library first
cd "$(dirname "$0")/.."
echo "Building libdarwin_compat..."
make clean || true
make

echo ""
echo "Building test program..."
cd tests

# Compile test program
cc -Wall -Wextra -I.. \
   -o test_shims test_shims.c \
   ../mach_port.c ../darwin_syscalls.c

echo "✓ Build successful"
echo ""

# Run tests
echo "Running tests..."
echo ""
./test_shims

echo ""
echo "✓ All tests completed successfully!"
