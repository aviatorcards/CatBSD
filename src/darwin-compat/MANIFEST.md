# Darwin Component Manifest

**CatBSD Darwin Compatibility Layer**  
**Last Updated**: January 15, 2026

This file tracks all Darwin components imported into CatBSD for integration with FreeBSD.

## Directory Structure

```
darwin-compat/
├── blocks/              # Blocks runtime (LLVM compiler-rt)
├── libdispatch/         # Grand Central Dispatch
├── CoreFoundation/      # Core framework
├── libsystem/           # System library compatibility shims
├── launchd/             # Init system (optional)
├── patches/             # FreeBSD compatibility patches
│   ├── blocks/
│   ├── libdispatch/
│   ├── CoreFoundation/
│   └── libsystem/
└── MANIFEST.md          # This file
```

## Imported Components

| Component    | Version | Source | Import Date | Build Status | Integration Status |
| ------------ | ------- | ------ | ----------- | ------------ | ------------------ |
| _(none yet)_ | -       | -      | -           | -            | -                  |

## Planned Imports

### Phase 3: High Priority

1. **Blocks Runtime**
   - Source: LLVM compiler-rt
   - URL: https://github.com/llvm/llvm-project
   - Path: `compiler-rt/lib/BlocksRuntime`
   - Priority: HIGH
   - Estimated Effort: 1-2 days

2. **libdispatch**
   - Source: swift-corelibs-libdispatch
   - URL: https://github.com/apple/swift-corelibs-libdispatch
   - Priority: HIGH
   - Estimated Effort: 3-5 days

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
