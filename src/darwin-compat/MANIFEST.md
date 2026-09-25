# Darwin Component Manifest

**CatBSD Darwin Compatibility Layer**  
**Last Updated**: September 25, 2026

This file tracks all Darwin components imported into CatBSD for integration with FreeBSD.

## Directory Structure

Present today:

```
darwin-compat/
├── blocks/              # libBlocksRuntime.a  ← imported from LLVM compiler-rt
│   ├── runtime.c                # Upstream, unmodified
│   ├── Block.h                  # Upstream, unmodified
│   ├── Block_private.h          # Upstream, unmodified
│   ├── config.h                 # CatBSD hand-written (replaces autoconf)
│   └── tests/
├── shims/               # libdarwin_compat.a
│   ├── mach_port.{c,h}          # Mach ports over kqueue
│   ├── darwin_syscalls.{c,h}    # Darwin syscall equivalents
│   ├── xpc_shim.{c,h}           # XPC over AF_UNIX
│   ├── xpc_compat.h             # Darwin-name aliases (non-Apple only)
│   └── tests/
├── libdispatch/         # libcatbsd_dispatch.a
│   ├── dispatch_shim.{c,h}      # GCD subset on pthreads
│   ├── dispatch_compat.h        # Darwin-name aliases (non-Apple only)
│   └── tests/
├── launchd/             # liblaunch.a
│   ├── plist_lite.{c,h}         # dependency-free XML plist reader
│   ├── launch_job.{c,h}         # job model + supervision
│   └── tests/
├── <name>-demo/         # the Essential 15 utilities
└── MANIFEST.md          # This file
```

Planned, once additional swift-corelibs sources are imported:

```
darwin-compat/
├── CoreFoundation/      # Core framework
├── libsystem/           # System library compatibility shims
└── patches/             # FreeBSD compatibility patches
    ├── libdispatch/
    ├── CoreFoundation/
    └── libsystem/
```

## Imported Components

| Component      | Version | Source | Import Date | Build Status | Integration Status |
| -------------- | ------- | ------ | ----------- | ------------ | ------------------ |
| Blocks Runtime | LLVM main (Sep 2026) | [llvm-project/compiler-rt/lib/BlocksRuntime](https://github.com/llvm/llvm-project/tree/main/compiler-rt/lib/BlocksRuntime) | 2026-09-25 | Compiles | Partial — builds and tests pass on macOS; FreeBSD not yet run |

## Clean-room Implementations

Separate from imports: these are CatBSD code written against the documented
Darwin API surface, not ported Apple source. They exist so the rest of the
tree has something to build against before the large imports happen, and
they carry no APSL obligations.

| Component              | Location                     | Covers                                                        | Build Status | Tested                       |
| ---------------------- | ---------------------------- | ------------------------------------------------------------- | ------------ | ---------------------------- |
| Mach port shim         | `shims/mach_port.{c,h}`      | Ports, queued messages, rights, port sets                     | Compiles     | macOS ✓                      |
| Darwin syscall shim    | `shims/darwin_syscalls.{c,h}`| `mach_absolute_time`, thread names, page size, `proc_pidinfo`  | Compiles     | macOS ✓                      |
| XPC shim               | `shims/xpc_shim.{c,h}`       | Typed messages, named services, correlated replies, async      | Compiles     | Linux (TSan+ASan) ✓, macOS ✓ |
| libdispatch subset     | `libdispatch/`               | Queues, barriers, groups, semaphores, once, apply, timers      | Compiles     | Linux (TSan+ASan) ✓, macOS ✓ |
| liblaunch              | `launchd/`                   | Job plists, supervision, KeepAlive, throttling, socket activation | Compiles  | Linux (ASan) ✓, macOS ✓     |

The Mach and syscall shims use kqueue and BSD `sysctl`, so they build on
macOS and FreeBSD only. The three newer components (XPC, libdispatch,
liblaunch) are POSIX-only — all five test suites now pass on macOS.

Each ships a Darwin-name compatibility header (`xpc_compat.h`,
`dispatch_compat.h`) that maps `xpc_*` / `dispatch_*` spellings onto the
prefixed symbols on non-Apple targets, so ported sources need an extra
`#include` rather than a rename pass.

## Planned Imports

### Phase 3: High Priority

1. ~~**Blocks Runtime**~~ ✓ **Done** (2026-09-25)
   - Imported from LLVM compiler-rt into `blocks/`
   - `libBlocksRuntime.a` builds; tests pass on macOS
   - FreeBSD build pending

2. **libdispatch**
   - Source: swift-corelibs-libdispatch
   - URL: https://github.com/apple/swift-corelibs-libdispatch
   - Priority: HIGH
   - Estimated Effort: 3-5 days
   - Note: a clean-room subset now exists in `libdispatch/` and covers
     queues, groups, semaphores, barriers and timer sources. The upstream
     import is still the right answer for API completeness (dispatch sources
     on fds, priority bands, `dispatch_io`); it is no longer a blocker for
     anything else in the tree.

3. **CoreFoundation**
   - Source: swift-corelibs-foundation
   - URL: https://github.com/apple/swift-corelibs-foundation
   - Priority: HIGH
   - Estimated Effort: 5-7 days

4. **Libsystem Shims**
   - Source: Custom implementation
   - Priority: HIGH
   - Estimated Effort: 3-5 days

### Phase 4: Medium Priority

5. **launchd**
   - Source: Apple OSS (or simplified implementation)
   - Priority: MEDIUM
   - Estimated Effort: 7-14 days
   - Note: the simplified path is underway — `launchd/` supervises jobs from
     real plists with KeepAlive, throttling and socket activation. Still
     missing before this is an init system: a control protocol (the XPC shim
     is the transport), a domain/session model, and PID 1 behaviour.

## Component Status Definitions

### Build Status

- **Not Started**: Component not yet imported
- **Imported**: Source code copied, not yet building
- **Building**: Compiles but may have errors
- **Compiles**: Builds successfully
- **Tested**: Passes unit tests
- **Integrated**: Fully integrated into build system

### Integration Status

- **Planned**: On roadmap, not started
- **In Progress**: Actively being worked on
- **Partial**: Some features working
- **Complete**: Fully functional
- **Stable**: Production-ready

## Import Workflow

To import a new component:

1. **Clone source**:

   ```bash
   # Example for libdispatch
   git clone https://github.com/apple/swift-corelibs-libdispatch.git /tmp/libdispatch
   ```

2. **Import using script**:

   ```bash
   ./scripts/import-darwin.sh libdispatch
   ```

3. **Update manifest**:
   - Add entry to "Imported Components" table
   - Set initial status

4. **Create README**:
   - Document porting notes
   - List dependencies
   - Track patches

5. **Begin integration**:
   - Review code for FreeBSD compatibility
   - Create patches as needed
   - Add to build system

## Patch Management

Patches are stored in `patches/<component>/` and should be:

- Numbered sequentially (001-description.patch)
- Well-documented with comments
- Submitted upstream when possible

Example:

```
patches/libdispatch/
├── 001-freebsd-kqueue-support.patch
├── 002-remove-mach-dependencies.patch
└── README.md
```

## Dependencies

### External Dependencies

Components may require:

- **ICU**: Unicode support (for CoreFoundation)
- **pthreads**: Threading (FreeBSD libthr)
- **kqueue**: Event notification (FreeBSD native)
- **Clang**: Blocks support (already available)

Install on FreeBSD:

```bash
pkg install icu
```

### Internal Dependencies

```
Blocks Runtime (no deps)
    ↓
libdispatch (needs Blocks, pthreads, kqueue)
    ↓
CoreFoundation (needs libdispatch, ICU)
    ↓
Libsystem (needs all above)
    ↓
launchd (needs Libsystem, CoreFoundation)
```

## Build Integration

Components will be integrated into the main CatBSD build:

```bash
# Build all Darwin components
make -C src/darwin-compat

# Build specific component
make -C src/darwin-compat/libdispatch

# Install to system
make -C src/darwin-compat install
```

## Testing

Each component should have:

- Unit tests (from upstream)
- Integration tests (CatBSD-specific)
- Compatibility tests (Darwin app testing)

Run tests:

```bash
make -C src/darwin-compat test
```

## Notes

- All components must maintain APSL/BSD license compatibility
- Prefer upstream sources over Apple OSS when available (e.g., swift-corelibs)
- Document all FreeBSD-specific changes
- Keep patches minimal and well-justified

## References

- [Porting Priorities](../docs/porting-priorities.md)
- [Codebase Comparison](../docs/codebase-comparison.md)
- [Porting Guide](../docs/porting-guide.md)
