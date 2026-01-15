# CatBSD Darwin Utilities Toolkit - Release Notes

## Version 0.1.0-alpha (January 2026)

**Status**: Proof of Concept / Alpha Release  
**Platform**: macOS (FreeBSD integration planned)

### What's New

This is the **first alpha release** of the CatBSD Darwin Utilities Toolkit - a proof-of-concept demonstrating Darwin API compatibility on FreeBSD.

### The Essential 15

Complete collection of 15 Darwin-compatible utilities:

**System & Configuration** (3)

- sw_vers - System version with cat ASCII art 🐱
- scutil - System configuration access
- networksetup - Network management

**File Operations** (4)

- plutil - Property list validation/parsing
- xattr - Extended file attributes
- ditto - File copying with metadata + archives
- open - File and URL opener

**Clipboard** (2)

- pbcopy - Copy stdin to clipboard
- pbpaste - Paste clipboard to stdout

**Media & Output** (3)

- say - Text-to-speech synthesis
- afplay - Audio file playback
- screencapture - Screenshot capture

**Management** (3)

- launchd - Service/daemon management
- defaults - User preferences system
- caffeinate - Prevent system sleep

### Technical Achievements

✅ **Compatibility Shim Library**

- Mach port → kqueue translation
- Darwin syscall compatibility
- Platform-specific adaptations

✅ **Build System**

- 15 independent Makefiles
- Unified build script
- Comprehensive test suites

✅ **Documentation**

- Individual READMEs for each utility
- Complete walkthrough
- Demo scripts

### Known Limitations

⚠️ **Alpha Status**

- macOS-only demos (FreeBSD integration pending)
- Wrapper-based implementations
- No binary plist support
- No package installation

⚠️ **Not Included**

- Bootable system
- Full Darwin frameworks
- GUI components
- Production stability

### Installation

```bash
git clone <repository>
cd CatBSD/src/darwin-compat
./build-all.sh
./demo-essential-15.sh
```

### Next Steps (v0.2.0-alpha)

Planned for next release:

- FreeBSD VM testing
- Additional utilities (target: 20+)
- Improved shim library
- Better FreeBSD compatibility

### Feedback

This is an **experimental alpha**. Expect:

- Breaking changes
- API instability
- Incomplete features
- Bugs and limitations

Report issues and suggestions via GitHub.

---

**Remember**: This is a proof-of-concept toolkit, not a production system!

🐾 CatBSD - Where FreeBSD meets Darwin
