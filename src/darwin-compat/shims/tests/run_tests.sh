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
echo "Building test programs..."
cd tests

# Mach port + Darwin syscall shims
cc -Wall -Wextra -I.. \
   -o test_shims test_shims.c \
   ../mach_port.c ../darwin_syscalls.c

# XPC shim. Built separately because it shares no state with the Mach
# shims and links pthreads; keeping the binaries apart means a failure
# points at one shim rather than "the tests".
cc -Wall -Wextra -I.. \
   -o test_xpc test_xpc.c \
   ../xpc_shim.c -lpthread

echo "✓ Build successful"
echo ""

# Run tests
echo "Running Mach port / syscall tests..."
echo ""
./test_shims

echo ""
echo "Running XPC tests..."
echo ""
./test_xpc

echo ""
echo "✓ All tests completed successfully!"
