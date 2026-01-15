# Darwin Compatibility Shim Library

This directory contains the CatBSD Darwin compatibility shim library (`libdarwin_compat`), which provides FreeBSD implementations of Darwin-specific APIs.

## Purpose

Darwin components often use macOS-specific APIs that don't exist on FreeBSD:

- Mach ports for IPC
- Darwin-specific syscalls
- XPC for high-level IPC
- IOKit for device management

The shim library provides FreeBSD equivalents for these APIs, allowing Darwin components to run on CatBSD with minimal code changes.

## Components

### mach_port.h/c

**Mach Port → kqueue Translation**

Maps Mach port operations to FreeBSD kqueue:

- `mach_port_allocate()` → `kqueue()`
- `mach_port_deallocate()` → `close()`
- `mach_msg_send()` → `kevent()` with NOTE_TRIGGER
- `mach_msg_receive()` → `kevent()` wait

**Limitations**:

- Simplified message passing (no full Mach message format)
- No port rights management
- Single-threaded event handling

**Usage**:

```c
#include "mach_port.h"

mach_port_t port;
mach_port_allocate(&port);

// Send message
char msg[] = "Hello";
mach_msg_send(port, msg, sizeof(msg));

// Receive message
char buf[256];
mach_msg_receive(port, buf, sizeof(buf), 1000); // 1s timeout

mach_port_deallocate(port);
```

### darwin_syscalls.h/c

**Darwin Syscall Compatibility**

Provides FreeBSD equivalents for common Darwin syscalls:

| Darwin Function            | FreeBSD Equivalent                |
| -------------------------- | --------------------------------- |
| `mach_absolute_time()`     | `clock_gettime(CLOCK_MONOTONIC)`  |
| `pthread_setname_np(name)` | `pthread_set_name_np(self, name)` |
| `vm_page_size`             | `getpagesize()`                   |
| `proc_pidinfo()`           | `sysctl()` (partial)              |

**Usage**:

```c
#include "darwin_syscalls.h"

// Get high-resolution time
uint64_t start = darwin_absolute_time();
// ... do work ...
uint64_t end = darwin_absolute_time();
uint64_t elapsed_ns = end - start;

// Set thread name
darwin_pthread_setname_np("worker-thread");

// Get page size
size_t page_size = darwin_vm_page_size();
```

### xpc_shim.h/c (TODO)

**XPC → Unix Domain Sockets**

Will provide XPC compatibility using Unix domain sockets:

- `xpc_connection_create()` → `socket(AF_UNIX)`
- `xpc_connection_send_message()` → `send()`
- `xpc_connection_set_event_handler()` → event loop

**Status**: Not yet implemented

## Building

### Build Library

```bash
cd src/darwin-compat/shims
make
```

This creates `libdarwin_compat.a`.

### Install Library

```bash
make install DESTDIR=/path/to/staging
```

Installs to:

- Library: `/usr/lib/libdarwin_compat.a`
- Headers: `/usr/include/darwin_compat/*.h`

### Clean

```bash
make clean
```

## Using in Components

### Makefile Integration

In your component's `Makefile.bsd`:

```makefile
# Include shim headers
CFLAGS+= -I../shims

# Link shim library
LDADD+= -L../shims -ldarwin_compat
DPADD+= ../shims/libdarwin_compat.a
```

### Source Code Integration

Use conditional compilation:

```c
#ifdef FREEBSD_COMPAT
#include "mach_port.h"
#include "darwin_syscalls.h"
#else
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

void my_function(void) {
#ifdef FREEBSD_COMPAT
    uint64_t time = darwin_absolute_time();
#else
    uint64_t time = mach_absolute_time();
#endif
    // ... rest of code ...
}
```

## Implementation Notes

### Design Principles

1. **Minimal Changes**: Shims should require minimal changes to Darwin code
2. **Best Effort**: Provide reasonable equivalents, even if not perfect
3. **Document Limitations**: Clearly document what's not supported
4. **Performance**: Avoid unnecessary overhead

### Current Limitations

**Mach Ports**:

- No full message format support
- No port rights management
- Simplified event handling
- Single-threaded only

**Syscalls**:

- `proc_pidinfo()` not fully implemented
- Some Darwin-specific features unavailable

**XPC**:

- Not yet implemented
- Will be simplified compared to real XPC

### Future Enhancements

1. **Improved Mach Port Emulation**
   - Message queuing
   - Port rights tracking
   - Multi-threaded support

2. **XPC Implementation**
   - Basic connection management
   - Message serialization
   - Event handling

3. **Additional Syscalls**
   - More complete `proc_pidinfo()`
   - Darwin-specific file operations
   - Additional thread operations

## Testing

### Unit Tests

Create tests in `tests/` subdirectory:

```c
// tests/test_mach_port.c
#include "mach_port.h"
#include <assert.h>

int main() {
    mach_port_t port;
    assert(mach_port_allocate(&port) == KERN_SUCCESS);
    assert(port != MACH_PORT_NULL);
    assert(mach_port_deallocate(port) == KERN_SUCCESS);
    return 0;
}
```

Build and run:

```bash
cc -I. tests/test_mach_port.c mach_port.c -o test_mach_port
./test_mach_port
```

## Contributing

When adding new shims:

1. **Create header file** with API declarations
2. **Implement in C file** using FreeBSD equivalents
3. **Add to Makefile** (SRCS and INCS)
4. **Document in this README**
5. **Create unit tests**
6. **Update component porting guides**

## References

- [Mach IPC Documentation](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/Mach/Mach.html)
- [FreeBSD kqueue(2)](https://www.freebsd.org/cgi/man.cgi?query=kqueue)
- [XPC Services](https://developer.apple.com/documentation/xpc)
- [Darwin Source](https://opensource.apple.com/)

---

For questions or issues, see the main CatBSD documentation.
