# CatBSD Architecture

System design and technical decisions for the CatBSD hybrid OS.

## Overview

CatBSD is a hybrid operating system that combines:

- **FreeBSD** as the base kernel and userland
- **Darwin/macOS components** for compatibility and enhanced features
- **Modern enhancements** for experimentation and learning

## Design Philosophy

### 1. FreeBSD First

FreeBSD provides the stable foundation:

- Proven kernel with excellent performance
- Comprehensive userland utilities
- Strong security features (Capsicum, MAC framework)
- Active development and support

### 2. Selective Darwin Integration

Import Darwin components strategically:

- Focus on userland libraries and frameworks
- Avoid proprietary or heavily macOS-specific code
- Maintain compatibility with FreeBSD kernel APIs

### 3. Experimental Mindset

This is a learning project:

- Try modern approaches (Rust utilities, new init systems)
- Document failures and successes
- Prioritize understanding over perfection

## System Architecture

```
┌─────────────────────────────────────────────┐
│           User Applications                  │
├─────────────────────────────────────────────┤
│  Darwin Userland  │  FreeBSD Userland       │
│  - launchd        │  - Standard utilities   │
│  - CoreFoundation │  - Shell tools          │
│  - libdispatch    │  - Network tools        │
├─────────────────────────────────────────────┤
│       Compatibility Layer (Shims)            │
│  - Syscall translation                       │
│  - API adaptation                            │
├─────────────────────────────────────────────┤
│         FreeBSD Kernel (Base)                │
│  + Selected XNU enhancements                 │
│  - VFS improvements                          │
│  - Modern features                           │
└─────────────────────────────────────────────┘
```

## Component Breakdown

### Kernel Layer

**Base**: FreeBSD kernel (`sys/`)

- Proven stability and performance
- Comprehensive driver support
- Modern features (ZFS, DTrace, jails)

**Enhancements from XNU**:

- VFS improvements (if portable)
- Performance optimizations
- Modern filesystem features

**Integration Strategy**:

1. Start with unmodified FreeBSD kernel
2. Cherry-pick non-proprietary XNU components
3. Create compatibility shims where needed
4. Test incrementally in VM

### Userland Layer

#### FreeBSD Components (Keep)

- Core utilities (ls, cp, mv, etc.)
- Shell (sh, csh)
- Network tools (ifconfig, route, etc.)
- Package management (pkg)

#### Darwin Components (Port)

**High Priority**:

- **launchd**: Modern init system replacing rc.d
- **libdispatch**: Grand Central Dispatch for concurrency
- **CoreFoundation**: Core data structures and utilities
- **libsystem**: System-level library integration

**Medium Priority**:

- **Security framework**: Keychain and authentication
- **IOKit basics**: Device management (if feasible)
- **Foundation**: Higher-level utilities

**Low Priority**:

- Additional frameworks as needed
- macOS-specific tools (for compatibility testing)

### Compatibility Layer

**Purpose**: Bridge differences between FreeBSD and Darwin APIs

**Components**:

- **Syscall shims**: Translate Mach calls to FreeBSD equivalents
- **API wrappers**: Adapt Darwin APIs to FreeBSD kernel
- **Library compatibility**: Ensure Darwin libs work with FreeBSD kernel

**Example**: Mach Ports → FreeBSD IPC

```c
// Shim layer translates Mach port operations
// to FreeBSD kqueue/pipes/sockets
mach_port_t port = mach_port_allocate(...);
// → Internally creates FreeBSD kqueue
```

## Directory Structure

### Source Organization

```
src/
├── freebsd/              # FreeBSD source tree (submodule)
│   ├── sys/              # Kernel
│   ├── lib/              # Libraries
│   ├── bin/              # Binaries
│   └── ...
├── darwin-compat/        # Ported Darwin components
│   ├── launchd/
│   ├── libdispatch/
│   ├── CoreFoundation/
│   └── libsystem/
├── patches/              # Integration patches
│   ├── freebsd/          # Patches to FreeBSD
│   └── darwin/           # Patches to Darwin components
├── kernel/               # Custom kernel configs
│   └── CATBSD            # CatBSD kernel configuration
└── userland/             # Custom utilities
    └── meow              # Example: cat facts utility
```

### Filesystem Layout

CatBSD will support both FreeBSD and macOS directory conventions:

```
/                         # Root
├── bin/                  # FreeBSD binaries
├── sbin/                 # FreeBSD system binaries
├── usr/                  # FreeBSD user utilities
├── Library/              # macOS-style library directory
│   ├── LaunchDaemons/    # launchd daemons
│   └── Frameworks/       # Darwin frameworks
├── System/               # macOS-style system directory
│   └── Library/          # System frameworks
└── Applications/         # macOS-style apps (future)
```

## Build System

### Build Pipeline

1. **FreeBSD Base Build**

   ```bash
   cd src/freebsd
   make buildworld buildkernel
   ```

2. **Darwin Component Build**

   ```bash
   cd src/darwin-compat
   # Custom build scripts for each component
   ./build-launchd.sh
   ./build-libdispatch.sh
   ```

3. **Integration**

   ```bash
   # Apply patches
   ./scripts/apply-patches.sh

   # Build complete system
   ./scripts/build-catbsd.sh
   ```

4. **ISO Generation**
   ```bash
   # Create bootable ISO
   make release
   ```

### Build Configuration

**Kernel Config** (`src/kernel/CATBSD`):

```
include GENERIC

# CatBSD-specific options
options     CATBSD
options     DARWIN_COMPAT

# Enhanced features
options     ZFS
options     DTRACE
```

## Integration Challenges

### Known Differences

| Aspect      | FreeBSD       | Darwin/XNU            |
| ----------- | ------------- | --------------------- |
| IPC         | kqueue, pipes | Mach ports            |
| Threading   | pthreads      | pthreads + GCD        |
| Init System | rc.d          | launchd               |
| Filesystem  | UFS, ZFS      | APFS, HFS+            |
| Security    | Capsicum, MAC | Sandbox, Entitlements |

### Resolution Strategies

1. **Syscall Translation**: Map Darwin syscalls to FreeBSD equivalents
2. **Library Shims**: Provide compatibility libraries
3. **Dual Support**: Support both FreeBSD and Darwin conventions
4. **Selective Porting**: Only port what's feasible

## Modern Enhancements

### Experimental Features

1. **Rust Utilities**: Rewrite some utilities in Rust
   - Example: `meow` command (cat facts)
   - Memory safety benefits
   - Learning opportunity

2. **Container Integration**: Enhance FreeBSD jails with macOS sandbox concepts
   - Hybrid isolation model
   - Modern security features

3. **Build System Modernization**: Use modern build tools
   - CMake for Darwin components
   - Ninja for faster builds

## Performance Considerations

### Optimization Targets

- **Boot Time**: Optimize init system (launchd)
- **Memory Usage**: Efficient library loading
- **I/O Performance**: Leverage ZFS and modern VFS

### Profiling Tools

- DTrace for system-wide profiling
- lldb for debugging
- Custom logging for integration points

## Security Model

### FreeBSD Security Features (Keep)

- Capsicum capability mode
- MAC framework
- Jails for isolation

### Darwin Security Features (Adapt)

- Sandbox profiles → Capsicum policies
- Entitlements → MAC labels
- Keychain → FreeBSD keyring

## Future Directions

### Phase 1: CLI System

- Bootable FreeBSD + Darwin userland
- Basic compatibility layer
- Core utilities working

### Phase 2: Enhanced Userland

- Full launchd integration
- CoreFoundation working
- macOS-compatible tools

### Phase 3: GUI Exploration

- X11/Wayland integration
- Quartz-like compositor
- Basic window management

### Phase 4: Application Support

- Cocoa compatibility layer
- Port simple macOS apps
- Developer tools

## References

- [FreeBSD Architecture Handbook](https://docs.freebsd.org/en/books/arch-handbook/)
- [XNU Source Code](https://github.com/apple-oss-distributions/xnu)
- [ravynOS Architecture](https://ravynos.com/architecture)
- [Darwin Documentation](https://developer.apple.com/library/archive/documentation/Darwin/Reference/ManPages/)
