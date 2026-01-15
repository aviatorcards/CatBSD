#!/bin/sh
# CatBSD Userland Analysis Script
# Compares FreeBSD and Darwin userland libraries

set -e

echo "=== CatBSD Userland Analysis ==="
echo ""

# Load environment
if [ -f .catbsd.env ]; then
    . ./.catbsd.env
else
    CATBSD_ROOT="$(pwd)"
    CATBSD_SRC="$CATBSD_ROOT/src"
fi

FREEBSD_SRC="$CATBSD_SRC/freebsd"
APPLE_SRC="$CATBSD_SRC/apple-oss"
REPORT_DIR="$CATBSD_ROOT/docs/reports"
REPORT="$REPORT_DIR/userland-comparison.md"

# Create reports directory
mkdir -p "$REPORT_DIR"

echo "Analyzing userland libraries..."
echo ""

# Helper function to check if directory/file exists
check_component() {
    local name=$1
    local path=$2
    
    if [ -d "$path" ] || [ -f "$path" ]; then
        echo "  ✓ Found: $name"
        return 0
    else
        echo "  ✗ Missing: $name"
        return 1
    fi
}

# Check FreeBSD components
echo "Checking FreeBSD userland..."
check_component "libc" "$FREEBSD_SRC/lib/libc"
check_component "libpthread" "$FREEBSD_SRC/lib/libthr"
check_component "libm" "$FREEBSD_SRC/lib/msun"
check_component "libutil" "$FREEBSD_SRC/lib/libutil"

echo ""
echo "Checking Apple OSS components..."

# Check for key Darwin components
check_component "Libsystem" "$APPLE_SRC/Libsystem"
check_component "libdispatch" "$APPLE_SRC/libdispatch"
check_component "CoreFoundation" "$APPLE_SRC/CoreFoundation"
check_component "launchd" "$APPLE_SRC/launchd"
check_component "Libc" "$APPLE_SRC/Libc"
check_component "libpthread" "$APPLE_SRC/libpthread"

echo ""

# Generate report
cat > "$REPORT" << 'EOF'
# Userland Library Comparison

Generated: $(date)

## Overview

This document compares FreeBSD and Darwin userland libraries to identify:
- Portable components for CatBSD
- API compatibility levels
- Integration challenges
- Porting priorities

## Library Architecture

### FreeBSD Userland

**Structure**: Traditional BSD userland
- **Location**: `lib/`, `libexec/`, `usr.bin/`, `usr.sbin/`
- **Key libraries**:
  - `libc` - C standard library
  - `libthr` - POSIX threads
  - `libm` - Math library
  - `libutil` - BSD utility functions
  - `libcrypt` - Cryptography
  - `libz` - Compression

### Darwin Userland

**Structure**: Layered library system
- **Location**: Various projects in apple-oss-distributions
- **Key components**:
  - `Libsystem` - Umbrella library (re-exports all system libs)
  - `Libc` - Darwin C library
  - `libpthread` - POSIX threads with extensions
  - `libdispatch` - Grand Central Dispatch (GCD)
  - `CoreFoundation` - Core framework
  - `launchd` - Init/service management system

## Component Analysis

### 1. Libsystem (Darwin)

**Purpose**: Umbrella library that re-exports all system libraries

**Components**:
- `libsystem_c.dylib` (libc)
- `libsystem_pthread.dylib` (threads)
- `libsystem_kernel.dylib` (syscall wrappers)
- `libsystem_malloc.dylib` (memory allocation)
- `libsystem_blocks.dylib` (blocks runtime)
- `libdispatch.dylib` (GCD)

**Porting Strategy**:
- Build as compatibility layer on FreeBSD
- Map to FreeBSD equivalents where possible
- Implement missing components (blocks, GCD)

**Priority**: HIGH
**Complexity**: Medium-High
**Dependencies**: Most other Darwin components

---

### 2. libdispatch (Grand Central Dispatch)

**Purpose**: Concurrent programming library

**Features**:
- Dispatch queues (serial, concurrent, global)
- Dispatch sources (timers, file descriptors, signals)
- Dispatch groups and semaphores
- Work items and barriers

**FreeBSD Equivalent**: None (use pthreads directly)

**Porting Status**: 
- ✅ Portable - libdispatch has been ported to Linux
- Uses pthreads and kqueue underneath
- Should work on FreeBSD with minimal changes

**Priority**: HIGH
**Complexity**: Medium
**Dependencies**: kqueue, pthreads

**Porting Notes**:
```
Dependencies:
- POSIX threads (FreeBSD: libthr)
- kqueue (FreeBSD: native support)
- Blocks runtime (need to port)
```

---

### 3. CoreFoundation

**Purpose**: Core framework providing fundamental data types and services

**Features**:
- CFString, CFArray, CFDictionary, etc.
- Run loops
- Property lists (plists)
- Bundles
- Preferences system

**FreeBSD Equivalent**: None

**Porting Status**:
- ✅ Partially portable - open source version available
- Some macOS-specific features may not work
- Core data types should port cleanly

**Priority**: HIGH
**Complexity**: Medium-High
**Dependencies**: libdispatch, ICU (Unicode)

**Porting Notes**:
```
Dependencies:
- libdispatch (for run loops)
- ICU library (Unicode support)
- pthreads
```

---

### 4. launchd

**Purpose**: Init system and service manager

**Features**:
- Process supervision
- On-demand service launching
- Socket activation
- Environment management
- Periodic jobs (like cron)

**FreeBSD Equivalent**: rc.d system

**Porting Status**:
- ⚠️ Complex - tightly integrated with macOS
- Depends on XPC (Mach IPC)
- May need significant rework

**Priority**: HIGH (for macOS compatibility)
**Complexity**: High
**Dependencies**: XPC, Libsystem, CoreFoundation

**Porting Strategy**:
- Option 1: Port launchd with XPC shims
- Option 2: Create launchd-compatible wrapper around rc.d
- Option 3: Simplified launchd implementation

---

### 5. Libc Comparison

| Feature | FreeBSD libc | Darwin Libc |
|---------|--------------|-------------|
| Base | BSD libc | BSD-derived |
| Thread-safe | Yes | Yes |
| Locale support | Full | Full |
| Extensions | BSD-specific | Darwin-specific |
| malloc | jemalloc | magazine malloc |
| stdio | Standard | Enhanced |

**Compatibility**: ~90% compatible at source level

**Key Differences**:
- Darwin has `_DARWIN_C_SOURCE` feature test macros
- Some function signatures differ slightly
- Darwin has additional functions (e.g., `copyfile()`)

**Porting Strategy**:
- Use FreeBSD libc as base
- Add Darwin compatibility shims
- Implement missing Darwin-specific functions

---

### 6. Threading Libraries

#### FreeBSD: libthr

- POSIX threads implementation
- 1:1 threading model (kernel threads)
- Good performance

#### Darwin: libpthread

- POSIX threads + extensions
- Workqueues integration
- QoS (Quality of Service) support
- Integrated with GCD

**Porting Strategy**:
- Use FreeBSD libthr as base
- Add Darwin extensions as compatibility layer
- Implement workqueues on top of thread pools

---

### 7. Blocks Runtime

**Purpose**: Support for C blocks (closures)

**Status**: Required for libdispatch and modern Objective-C

**Porting**:
- ✅ Available as separate project (compiler-rt)
- Clang already supports blocks syntax
- Just need to link runtime library

**Priority**: HIGH (dependency for GCD)
**Complexity**: Low (already portable)

## Porting Priorities

### Phase 3: High Priority

1. **Blocks Runtime** ✅
   - Complexity: Low
   - Time: 1-2 days
   - Blocker for: libdispatch

2. **libdispatch** ✅
   - Complexity: Medium
   - Time: 3-5 days
   - Blocker for: CoreFoundation, many apps

3. **CoreFoundation** ⚠️
   - Complexity: Medium-High
   - Time: 5-7 days
   - Blocker for: Most macOS apps

4. **Libsystem Shims** ⚠️
   - Complexity: Medium
   - Time: 3-5 days
   - Blocker for: Darwin binary compatibility

### Phase 4: Medium Priority

5. **launchd** ⚠️
   - Complexity: High
   - Time: 7-14 days
   - Alternative: Use rc.d with compatibility layer

6. **Darwin libc Extensions** ✅
   - Complexity: Low-Medium
   - Time: 2-4 days
   - Incremental additions as needed

### Future: Low Priority

7. **Additional Frameworks**
   - Security framework
   - CFNetwork
   - Foundation (requires Objective-C runtime)

## Build System Integration

### Recommended Approach

```
src/darwin-compat/
├── blocks/              # Blocks runtime
├── libdispatch/         # GCD
├── CoreFoundation/      # Core framework
├── libsystem/           # Compatibility shims
├── launchd/             # Init system (optional)
└── Makefile.darwin      # Build orchestration
```

### Build Order

1. Blocks runtime
2. libdispatch (depends on blocks)
3. CoreFoundation (depends on libdispatch)
4. Libsystem shims (depends on above)
5. launchd (depends on all above)

## API Compatibility Matrix

| API Category | FreeBSD | Darwin | Compatibility |
|--------------|---------|--------|---------------|
| POSIX basics | ✅ | ✅ | 100% |
| BSD extensions | ✅ | ✅ | ~95% |
| Threads (pthread) | ✅ | ✅ | ~90% |
| Dispatch (GCD) | ❌ | ✅ | 0% (need port) |
| CoreFoundation | ❌ | ✅ | 0% (need port) |
| Blocks | ⚠️ | ✅ | 100% (with runtime) |
| kqueue | ✅ | ✅ | ~95% |
| Mach IPC | ❌ | ✅ | 0% (need emulation) |

## Testing Strategy

### Unit Tests

- Port existing libdispatch tests
- Port CoreFoundation tests
- Create integration tests

### Integration Tests

- Build simple Darwin apps
- Test launchd plist compatibility
- Verify GCD functionality

### Compatibility Tests

- Run macOS command-line tools
- Test property list handling
- Verify run loop behavior

## Recommendations

### For CatBSD Phase 3

1. ✅ **Start with blocks runtime** - easy win, enables GCD
2. ✅ **Port libdispatch next** - high value, well-tested on Linux
3. ✅ **Add CoreFoundation** - essential for macOS app compatibility
4. ⚠️ **Defer launchd** - complex, can use rc.d initially
5. ✅ **Build incrementally** - test each component thoroughly

### Success Criteria

- [ ] Blocks runtime compiles and passes tests
- [ ] libdispatch compiles and passes test suite
- [ ] CoreFoundation compiles and basic tests pass
- [ ] Simple Darwin CLI tools run successfully
- [ ] Property lists can be read/written

## References

- libdispatch source: https://github.com/apple/swift-corelibs-libdispatch
- CoreFoundation source: https://github.com/apple/swift-corelibs-foundation
- Blocks runtime: LLVM compiler-rt
- FreeBSD Porter's Handbook

## Next Steps

1. Create import script for Darwin components
2. Set up build infrastructure
3. Begin with blocks runtime
4. Port libdispatch
5. Integrate CoreFoundation
EOF

echo "  ✓ Created $REPORT"
echo ""
echo "=== Analysis Complete ==="
echo ""
echo "Report generated: $REPORT"
echo ""
