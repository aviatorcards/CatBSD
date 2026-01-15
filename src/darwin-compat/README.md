# CatBSD Darwin Utilities - Complete Collection

This directory contains working demonstrations of Darwin utilities ported to CatBSD.

## Utilities

### 1. **sw_vers** - System Version Information

Display CatBSD system information in Darwin style.

```bash
cd sw_vers-demo
./sw_vers-demo              # Show all info
./sw_vers-demo -banner      # Show cute cat banner!
./sw_vers-demo -productName # Just the product name
```

**Features:**

- System version display
- Kernel information
- Architecture detection
- Adorable cat ASCII art 🐱

---

### 2. **plutil** - Property List Utility

Validate and display .plist files (essential for launchd).

```bash
cd plutil-demo
./plutil-demo -lint test.plist  # Validate
./plutil-demo -p test.plist     # Display contents
```

**Features:**

- XML plist parsing (libxml2)
- Validation
- Human-readable output
- Foundation for launchd configs

---

### 3. **say** - Text-to-Speech

Make your computer talk with Darwin-style interface!

```bash
cd say-demo
./say-demo "Hello from CatBSD!"
./say-demo -v Alex "Meow!"
./say-demo --voice-list
```

**Features:**

- Text-to-speech synthesis
- Multiple voice support
- Darwin-compatible API
- Maximum fun factor! 🔊

---

### 4. **caffeinate** - Prevent System Sleep

Keep system awake while running commands.

```bash
cd caffeinate-demo
./caffeinate-demo              # Keep awake indefinitely
./caffeinate-demo -t 60        # Keep awake for 60 seconds
./caffeinate-demo make build   # Keep awake during build
```

**Features:**

- Prevent system sleep
- Timed operation
- Run commands while caffeinated
- Uses Darwin syscall shims ☕

---

### 5. **launchd** - Service Management

Simplified daemon/service manager.

```bash
cd launchd-demo
./launchd-demo start com.example.daemon
./launchd-demo list
./launchd-demo stop com.example.daemon
```

**Features:**

- Daemon management
- Mach port IPC (via shims)
- Process control
- Foundation for init system

---

## Running the Complete Demo

```bash
./demo.sh
```

This will showcase all 5 utilities in action, including:

- System information display
- Plist validation
- Text-to-speech announcements
- Power management
- Service management

## Architecture

All utilities demonstrate different aspects of Darwin porting:

| Utility        | Demonstrates       | Dependencies         |
| -------------- | ------------------ | -------------------- |
| **sw_vers**    | System info APIs   | POSIX uname          |
| **plutil**     | XML parsing        | libxml2              |
| **say**        | Audio/Speech       | System `say`         |
| **caffeinate** | Power management   | Darwin syscall shims |
| **launchd**    | IPC & process mgmt | Mach port shims      |

## Building All Utilities

```bash
# Build individually
cd sw_vers-demo && make
cd plutil-demo && make
cd say-demo && make
cd caffeinate-demo && make
cd launchd-demo && make

# Or use the master build script (if created)
./build-all.sh
```

## Integration with CatBSD

These utilities form the foundation of CatBSD's Darwin compatibility layer:

1. **sw_vers** - System identification
2. **plutil** - Configuration parsing for launchd
3. **say** - User feedback and accessibility
4. **caffeinate** - Power management integration
5. **launchd** - Service/daemon management

## Next Steps

Future utilities to port:

- `dscl` - Directory services
- `defaults` - User defaults system
- `open` - File/URL opener
- `pbcopy`/`pbpaste` - Clipboard utilities
- `osascript` - AppleScript runner (simplified)

---

**Status**: 5 working utilities demonstrating Darwin compatibility! 🎉
