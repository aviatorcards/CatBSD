# CatBSD Darwin Utilities Toolkit

[![Version](https://img.shields.io/badge/version-0.1.0--alpha-orange)](https://github.com/aviatorcards/CatBSD/releases)
[![License](https://img.shields.io/badge/license-BSD%2FAPSL-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20FreeBSD-lightgrey)](https://github.com/aviatorcards/CatBSD)
[![Status](https://img.shields.io/badge/status-alpha%20%7C%20proof--of--concept-yellow)](https://github.com/aviatorcards/CatBSD)

**Version**: 0.1.0-alpha  
**Status**: Proof of Concept / Alpha  
**License**: Mixed (BSD/APSL - see individual components)

## What is This?

A **proof-of-concept toolkit** demonstrating Darwin API compatibility on FreeBSD. This is an **alpha release** showing that macOS-style utilities can run on FreeBSD using compatibility shims.

> ⚠️ **Alpha Software**: This is experimental. Not production-ready. APIs may change.

---

## 🎯 The Essential 15

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

---

## 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/aviatorcards/CatBSD.git
cd CatBSD

# Build all utilities
cd src/darwin-compat
./build-all.sh

# Run comprehensive demo
./demo-essential-15.sh

# Try individual utilities
cd sw_vers-demo && ./sw_vers-demo
```

---

## ✅ What Works

- All 15 utilities compile and run on macOS
- Compatibility shim library (Mach ports → kqueue)
- Darwin-compatible command-line interfaces
- Comprehensive test suites

## ❌ What Doesn't Work (Yet)

- FreeBSD integration (currently macOS-only demos)
- Binary plist support
- Full framework implementations
- Bootable system
- Package management

---

## 🏗️ Architecture

```
Darwin Utilities
    ↓
Compatibility Shims (mach_port, darwin_syscalls)
    ↓
FreeBSD/macOS Native APIs
```

**Key Components:**

- **Shim Library** - Translates Darwin APIs to FreeBSD equivalents
- **Utilities** - 15 command-line tools with Darwin interfaces
- **Build System** - Unified build and test infrastructure

---

## 📚 Documentation

- [Setup Guide](docs/setup.md)
- [Architecture](docs/architecture.md)
- [Porting Guide](docs/porting-guide.md)
- [Build Guide](docs/build-guide.md)
- [Contributing](CONTRIBUTING.md)

---

## 🗺️ Roadmap

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

---

## 🤝 Contributing

This is an **experimental proof-of-concept**. Contributions welcome but expect breaking changes.

See [CONTRIBUTING.md](CONTRIBUTING.md) for development workflow and guidelines.

---

## 📝 License

Mixed licensing:

- CatBSD code: BSD-2-Clause
- Darwin components: Apple Public Source License (APSL)
- See individual files for details

---

## 🙏 Acknowledgments

Inspired by [ravynOS](https://ravynos.com/) and the FreeBSD community.

Special thanks to the open-source community for making projects like this possible.

---

## 🐾 CatBSD

**Where FreeBSD meets Darwin**

_Alpha Release - Proof of Concept - Not Production Ready_

---

**Links:**

- [Releases](https://github.com/aviatorcards/CatBSD/releases)
- [Issues](https://github.com/aviatorcards/CatBSD/issues)
- [Discussions](https://github.com/aviatorcards/CatBSD/discussions)
