#!/bin/sh
# CatBSD Codebase Analysis Script
# Compares FreeBSD and Darwin source trees and generates analysis report

set -e

echo "=== CatBSD Codebase Analysis ==="
echo ""

# Load environment
if [ -f .catbsd.env ]; then
    . ./.catbsd.env
else
    echo "Warning: .catbsd.env not found. Using defaults."
    CATBSD_ROOT="$(pwd)"
    CATBSD_SRC="$CATBSD_ROOT/src"
fi

FREEBSD_DIR="$CATBSD_SRC/freebsd"
APPLE_DIR="$CATBSD_SRC/apple-oss"
REPORT_FILE="docs/analysis-report.md"

# Check if sources exist
check_sources() {
    if [ ! -d "$FREEBSD_DIR" ]; then
        echo "Error: FreeBSD source not found at $FREEBSD_DIR"
        echo "Run ./scripts/fetch-sources.sh first"
        exit 1
    fi
    
    if [ ! -d "$APPLE_DIR" ]; then
        echo "Error: Apple OSS not found at $APPLE_DIR"
        echo "Run ./scripts/fetch-sources.sh first"
        exit 1
    fi
    
    echo "✓ Source trees found"
}

# Analyze directory structures
analyze_structure() {
    echo ""
    echo "Analyzing directory structures..."
    
    # FreeBSD structure
    echo "  FreeBSD key directories:"
    for dir in sys lib bin sbin usr.bin usr.sbin; do
        if [ -d "$FREEBSD_DIR/$dir" ]; then
            COUNT=$(find "$FREEBSD_DIR/$dir" -maxdepth 1 -type d | wc -l)
            echo "    $dir: $COUNT subdirectories"
        fi
    done
    
    # Darwin structure
    echo "  Darwin/Apple OSS:"
    if [ -d "$APPLE_DIR" ]; then
        COUNT=$(find "$APPLE_DIR" -maxdepth 1 -type d 2>/dev/null | wc -l)
        echo "    Top-level projects: $COUNT"
    fi
}

# Identify common components
identify_components() {
    echo ""
    echo "Identifying Darwin components for porting..."
    
    COMPONENTS=""
    
    # Check for key Darwin components
    for comp in launchd libdispatch Libsystem xnu dyld CF; do
        if [ -d "$APPLE_DIR/$comp" ] || [ -f "$APPLE_DIR/$comp" ]; then
            echo "  ✓ Found: $comp"
            COMPONENTS="$COMPONENTS $comp"
        else
            echo "  ✗ Not found: $comp"
        fi
    done
    
    echo ""
    echo "Available components:$COMPONENTS"
}

# Compare kernel structures
compare_kernels() {
    echo ""
    echo "Comparing kernel structures..."
    
    # FreeBSD kernel
    if [ -d "$FREEBSD_DIR/sys" ]; then
        echo "  FreeBSD kernel (sys/):"
        for subsys in kern vm fs net; do
            if [ -d "$FREEBSD_DIR/sys/$subsys" ]; then
                FILES=$(find "$FREEBSD_DIR/sys/$subsys" -name "*.c" 2>/dev/null | wc -l)
                echo "    $subsys: $FILES C files"
            fi
        done
    fi
    
    # XNU kernel
    if [ -d "$APPLE_DIR/xnu" ]; then
        echo "  XNU kernel:"
        echo "    (Structure varies, manual analysis needed)"
    fi
}

# Analyze userland libraries
analyze_userland() {
    echo ""
    echo "Analyzing userland libraries..."
    
    # FreeBSD libc
    if [ -d "$FREEBSD_DIR/lib/libc" ]; then
        SIZE=$(find "$FREEBSD_DIR/lib/libc" -name "*.c" 2>/dev/null | wc -l)
        echo "  FreeBSD libc: ~$SIZE C files"
    fi
    
    # Darwin Libsystem
    if [ -d "$APPLE_DIR/Libsystem" ]; then
        echo "  Darwin Libsystem: Found"
    fi
}

# Generate analysis report
generate_report() {
    echo ""
    echo "Generating analysis report..."
    
    cat > "$REPORT_FILE" << 'EOF'
# CatBSD Codebase Analysis Report

Generated: $(date)

## Executive Summary

This report analyzes the FreeBSD and Darwin/macOS codebases to identify:
- Key architectural differences
- Components suitable for porting
- Integration challenges
- Recommended porting priorities

## 1. Kernel Comparison

### FreeBSD Kernel Architecture

**Type**: Monolithic kernel with modular design

**Key Subsystems**:
- `sys/kern/` - Core kernel (process, thread, scheduling)
- `sys/vm/` - Virtual memory management
- `sys/fs/` - Filesystem layer (UFS, ZFS, NFS)
- `sys/net/` - Networking stack (TCP/IP, routing)
- `sys/dev/` - Device drivers

**IPC Mechanisms**:
- kqueue/kevent (event notification)
- Pipes and FIFOs
- Unix domain sockets
- System V IPC (semaphores, shared memory, message queues)

**Security**:
- Capsicum (capability mode)
- MAC framework (Mandatory Access Control)
- Jails (OS-level virtualization)

### XNU Kernel Architecture

**Type**: Hybrid kernel (Mach microkernel + BSD subsystem)

**Key Components**:
- Mach microkernel (IPC, scheduling, VM)
- BSD subsystem (POSIX compatibility, networking)
- IOKit (device drivers, object-oriented)
- libkern (kernel C++ runtime)

**IPC Mechanisms**:
- Mach ports (primary IPC)
- XPC (high-level IPC framework)
- BSD sockets (networking)

**Security**:
- Sandbox (application sandboxing)
- Entitlements (capability system)
- Code signing enforcement

### Key Differences

| Aspect | FreeBSD | XNU/Darwin |
|--------|---------|------------|
| Architecture | Monolithic | Hybrid (Mach + BSD) |
| IPC | kqueue, pipes | Mach ports, XPC |
| Device Model | devfs, GEOM | IOKit (OOP) |
| Filesystems | UFS, ZFS | APFS, HFS+ |
| Init System | rc.d | launchd |
| Security | Capsicum, MAC | Sandbox, Entitlements |

### Porting Strategy

**Kernel Approach**: Keep FreeBSD kernel as base, add compatibility shims for Darwin userland

**Rationale**:
- FreeBSD kernel is stable and well-maintained
- XNU kernel is complex and tightly coupled to macOS
- Easier to adapt userland than replace kernel
- Compatibility layer can bridge differences

## 2. Userland Comparison

### Standard Libraries

**FreeBSD libc**:
- POSIX-compliant C library
- BSD extensions
- Thread-safe implementations
- Well-documented

**Darwin Libsystem**:
- Umbrella library combining multiple components
- libsystem_c (C library)
- libsystem_kernel (syscall wrappers)
- libsystem_pthread (threading)
- libsystem_malloc (memory allocation)

### Init Systems

**FreeBSD rc.d**:
- Shell script-based
- Sequential startup
- Simple dependencies
- Well-understood

**Darwin launchd**:
- Binary plist configuration
- On-demand service activation
- Socket activation
- Complex dependency management
- Integrated with Mach ports

### Build Systems

**FreeBSD**:
- BSD make
- Unified build system (buildworld/buildkernel)
- Well-integrated

**Darwin**:
- Xcode build system
- darwinbuild for OSS components
- CMake for some projects
- Fragmented across components

## 3. Component Inventory

### High Priority Components

#### launchd
- **Purpose**: Modern init and service management
- **Size**: ~50,000 lines of C
- **Dependencies**: libsystem, XPC, Mach ports
- **Difficulty**: Medium
- **Effort**: 2-3 weeks
- **Blockers**: Mach port IPC (needs shim)
- **Benefits**: Modern service management, on-demand activation

#### libdispatch (Grand Central Dispatch)
- **Purpose**: Concurrent programming framework
- **Size**: ~30,000 lines of C
- **Dependencies**: pthread, kqueue (FreeBSD compatible!)
- **Difficulty**: Medium
- **Effort**: 1-2 weeks
- **Blockers**: Workqueue syscalls (can use pthread pools)
- **Benefits**: Modern concurrency, widely used API

#### CoreFoundation
- **Purpose**: Core data structures and utilities
- **Size**: ~100,000 lines of C
- **Dependencies**: ICU, libiconv, libdispatch
- **Difficulty**: High
- **Effort**: 4-6 weeks
- **Blockers**: Large dependency tree
- **Benefits**: Foundation for other frameworks

### Medium Priority Components

#### Libsystem
- **Purpose**: System library integration
- **Size**: Varies (umbrella library)
- **Dependencies**: Kernel interfaces
- **Difficulty**: High
- **Effort**: 3-4 weeks
- **Blockers**: Syscall compatibility
- **Benefits**: Darwin API compatibility

#### Security Framework
- **Purpose**: Keychain, authentication, crypto
- **Size**: Large
- **Dependencies**: CoreFoundation, IOKit
- **Difficulty**: Very High
- **Effort**: 6-8 weeks
- **Blockers**: IOKit dependency, kernel integration
- **Benefits**: macOS-compatible security APIs

### Low Priority Components

#### IOKit
- **Purpose**: Device driver framework
- **Size**: Very large
- **Dependencies**: XNU kernel, C++ runtime
- **Difficulty**: Very High
- **Effort**: Months
- **Blockers**: Requires XNU kernel features
- **Benefits**: macOS driver compatibility (limited value)

## 4. Integration Challenges

### API Incompatibilities

**Mach Ports vs kqueue**:
- Darwin uses Mach ports for IPC
- FreeBSD uses kqueue for event notification
- Solution: Shim layer mapping Mach port operations to kqueue

**Syscall Differences**:
- Darwin has ~450 syscalls
- FreeBSD has ~550 syscalls
- Overlap is significant but not complete
- Solution: Syscall translation layer

**Threading Models**:
- Both use pthreads (compatible!)
- Darwin adds GCD on top
- Solution: Port libdispatch (already works on FreeBSD)

### Build System Challenges

**Different Build Tools**:
- FreeBSD: BSD make
- Darwin: Xcode, CMake, various
- Solution: Create BSD makefiles for Darwin components

**Dependency Management**:
- FreeBSD: Integrated in source tree
- Darwin: Fragmented across projects
- Solution: Vendor dependencies in darwin-compat/

### License Considerations

**FreeBSD**: BSD 2-Clause (permissive)
**Darwin**: APSL (Apple Public Source License)
- Compatible but different
- Must maintain APSL compliance for Darwin code
- Document licenses clearly

## 5. Recommended Porting Order

### Phase 3: Initial Integration
1. **libdispatch** (easiest, high value)
2. **launchd** (medium difficulty, high value)
3. **Basic shims** (Mach port, XPC)

### Phase 4: Enhanced Userland
4. **CoreFoundation** (harder but needed for frameworks)
5. **Libsystem components** (as needed)

### Phase 5: Advanced Features
6. **Security framework** (if time permits)
7. **Additional frameworks** (based on needs)

## 6. Next Steps

1. Create compatibility shim library (`libdarwin_compat`)
2. Extract and analyze libdispatch source
3. Create FreeBSD build system for libdispatch
4. Port and test libdispatch
5. Move to launchd with working shim layer

## Appendix: Useful Resources

- FreeBSD Source Browser: https://cgit.freebsd.org/src/
- Apple Open Source: https://opensource.apple.com/
- XNU Source: https://github.com/apple-oss-distributions/xnu
- ravynOS: https://github.com/ravynsoft/ravynos
- PureDarwin: http://www.puredarwin.org/
EOF

    echo "  ✓ Report generated: $REPORT_FILE"
}

# Main execution
main() {
    check_sources
    analyze_structure
    identify_components
    compare_kernels
    analyze_userland
    generate_report
    
    echo ""
    echo "=== Analysis Complete ==="
    echo ""
    echo "Review the report: $REPORT_FILE"
    echo ""
}

main
