# launchd for CatBSD - Simplified Demo

This is a simplified demonstration of porting launchd to CatBSD using our compatibility shim library.

## About

launchd is macOS's init system and service manager. Full launchd is ~50k lines and deeply integrated with macOS. For CatBSD, we're creating a **simplified demonstration** that shows:

1. How to use our Mach port shims
2. Basic daemon management concepts
3. Integration with FreeBSD

This is NOT a full launchd port - it's a proof-of-concept showing the porting workflow.

## What This Demo Includes

- **Simple daemon launcher** - Start/stop daemons
- **Mach port IPC** - Using our shim library
- **Basic plist parsing** - Read .plist configuration files
- **Process management** - Fork/exec daemons

## What's NOT Included

- XPC (complex IPC framework)
- Socket activation
- Complex dependency management
- Full macOS compatibility

## Building

```bash
cd src/darwin-compat/launchd-demo
make
```

## Running

```bash
# Start a simple daemon
./launchd-demo start com.example.daemon

# Stop a daemon
./launchd-demo stop com.example.daemon

# List running daemons
./launchd-demo list
```

## Integration with CatBSD Shims

This demo uses our compatibility shims:

```c
#include "mach_port.h"      // Mach port → kqueue
#include "darwin_syscalls.h" // Darwin syscalls
```

## Files

- `launchd-demo.c` - Main program
- `daemon.c` - Daemon management
- `plist.c` - Simple plist parser
- `Makefile` - Build system

## Next Steps

For a production launchd port:

1. Port full launchd source
2. Implement XPC shim
3. Add socket activation
4. Integrate with FreeBSD rc.d

---

**Status**: Demo implementation to prove porting workflow
