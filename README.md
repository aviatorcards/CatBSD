# CatBSD Darwin Utilities Toolkit

**Version**: 0.1.0-alpha  
**Status**: Proof of Concept / Alpha  
**License**: Mixed (BSD/APSL - see individual components)

## What is This?

A **proof-of-concept toolkit** demonstrating Darwin API compatibility on FreeBSD. This is an **alpha release** showing that macOS-style utilities can run on FreeBSD using compatibility shims.

> ⚠️ **Alpha Software**: This is experimental. Not production-ready. APIs may change.

## The Essential 15

A collection of 15 Darwin-compatible command-line utilities:

### System & Configuration

- `sw_vers` - System version information
- `scutil` - System configuration
- `networksetup` - Network management

### File Operations

- `plutil` - Property list utility
- `xattr` - Extended attributes
- `ditto` - File copying with metadata
- `open` - File/URL opener

### Clipboard

- `pbcopy` - Copy to clipboard
- `pbpaste` - Paste from clipboard

### Media & Output

- `say` - Text-to-speech
- `afplay` - Audio player
- `screencapture` - Screenshots

### Management

- `launchd` - Service management
- `defaults` - User preferences
- `caffeinate` - Power management

## Quick Start

```bash
# Build all utilities
cd src/darwin-compat
./build-all.sh

# Run demo
./demo-essential-15.sh

# Try individual utilities
cd sw_vers-demo && ./sw_vers-demo
cd plutil-demo && ./plutil-demo -help
```

## What Works

✅ All 15 utilities compile and run on macOS  
✅ Compatibility shim library (Mach ports → kqueue)  
✅ Darwin-compatible command-line interfaces  
✅ Comprehensive test suites

## What Doesn't Work (Yet)

❌ FreeBSD integration (currently macOS-only demos)  
❌ Binary plist support  
❌ Full framework implementations  
❌ Bootable system  
❌ Package management

## Architecture

```
Utilities
    ↓
Compatibility Shims (mach_port, darwin_syscalls)
    ↓
FreeBSD/macOS Native APIs
```

## Installation (Alpha)

**Current**: Manual build only

```bash
cd src/darwin-compat
./build-all.sh
# Binaries in each *-demo/ directory
```

**Future**: Package manager integration

## Roadmap

### v0.2.0-alpha

- More utilities (20+ total)
- FreeBSD VM testing
- Improved shim library

### v0.5.0-beta

- FreeBSD boot integration
- Package system
- Basic GUI exploration

### v1.0.0 (Future)

- Bootable FreeBSD system
- Stable Darwin compatibility layer
- Full userland integration

## Contributing

This is an **experimental proof-of-concept**. Contributions welcome but expect breaking changes.

## License

Mixed licensing:

- CatBSD code: BSD-2-Clause
- Darwin components: Apple Public Source License (APSL)
- See individual files for details

## Acknowledgments

Inspired by [ravynOS](https://ravynos.com/) and the FreeBSD community.

---

**CatBSD**: Where FreeBSD meets Darwin 🐾

_Alpha Release - Proof of Concept - Not Production Ready_
