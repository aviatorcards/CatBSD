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

### xpc_shim.h/c

**XPC → Unix Domain Sockets**

| Darwin                                  | This shim                                       |
| --------------------------------------- | ----------------------------------------------- |
| `xpc_connection_create(name)`           | `connect()` to a per-uid runtime dir socket     |
| `xpc_connection_create_listener(name)`  | `bind()` + `listen()`, name claimed with `flock` |
| `xpc_dictionary_*`                      | Typed, ref-counted message dictionary            |
| `xpc_connection_send_message()`         | Length-prefixed frame over `SOCK_STREAM`         |
| `..._with_reply_sync()`                 | Correlation id matched against queued traffic    |
| `xpc_connection_set_event_handler()`    | Reader thread invoking a callback                |

Everything is prefixed `catbsd_xpc_`, because macOS already ships real XPC in
libSystem — the same collision `mach_port.h` documents for `mach_port_t`.
Non-Apple targets that want Darwin spelling can include `xpc_compat.h`.

**Usage**:

```c
#include "xpc_shim.h"

int err;
catbsd_xpc_connection_t c = catbsd_xpc_connection_create("com.catbsd.jobd", &err);

catbsd_xpc_object_t req = catbsd_xpc_dictionary_create();
catbsd_xpc_dictionary_set_string(req, "op", "status");
catbsd_xpc_dictionary_set_int64(req, "pid", 4242);

catbsd_xpc_object_t reply = NULL;
if (catbsd_xpc_connection_send_message_with_reply_sync(c, req, 3000, &reply)
        == CATBSD_XPC_SUCCESS) {
    const char *state = catbsd_xpc_dictionary_get_string(reply, "state");
    /* ... */
    catbsd_xpc_release(reply);
}
catbsd_xpc_release(req);
catbsd_xpc_connection_release(c);
```

**Deliberate differences from Darwin XPC**:

- **Getters distinguish absent from zero.** `xpc_dictionary_get_int64()` on
  Darwin returns 0 for both a missing key and a genuine 0. Here the scalar
  getters return `CATBSD_XPC_EINVAL` and leave the out-param untouched
  unless the key exists *and* holds that exact type — a supervisor deciding
  whether a job set `KeepAlive` needs to tell those apart.
- **A service name is one path component**, resolved under
  `$CATBSD_XPC_RUNTIME_DIR` (default `/tmp/catbsd-xpc-<uid>`). Names
  containing `/` are rejected; an absolute name is used as a literal path.
- **No object graph.** Dictionaries hold scalars, strings and data — no
  nested dictionaries or arrays, no fd or endpoint passing.
- **No bootstrap namespace, no launchd-published services, no
  entitlements or peer credential checks.** A listener owns its name for as
  long as it holds the lock; that is the whole security model.

**Stale-name handling**: a bound AF_UNIX path outlives the process that
created it, so a crashed listener leaves one behind forever. The obvious
liveness test — connect and see if it succeeds — is actively harmful here:
on a `SOCK_STREAM` listener the probe connection is queued in the accept
backlog, so the live owner's next `accept()` returns a dead peer instead of
its real client. Instead each listener holds an `flock` on a sidecar
`<socket>.lock` file, which the kernel drops when the holder dies. Same
question, no side effects.

**Status**: implemented and tested (`tests/test_xpc.c`), clean under TSan
and ASan.

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

- Flat messages only: no nested dictionaries, arrays, fds or endpoints
- No bootstrap namespace, entitlements or peer credential checks
- 1 MiB frame cap (a peer is untrusted input; without a cap a bogus length
  prefix is an unbounded `malloc`)

### Future Enhancements

1. **Improved Mach Port Emulation**
   - Message queuing
   - Port rights tracking
   - Multi-threaded support

2. **Richer XPC**
   - Nested containers (dictionaries and arrays as values)
   - File descriptor passing via `SCM_RIGHTS`
   - Peer credentials (`SO_PEERCRED` / `LOCAL_PEERCRED`) for authorization

3. **Additional Syscalls**
   - More complete `proc_pidinfo()`
   - Darwin-specific file operations
   - Additional thread operations

## Testing

### Unit Tests

```bash
cd tests && ./run_tests.sh    # builds the library, then both suites
# or, from this directory:
make test
```

Two binaries, deliberately kept separate so a failure points at one shim
rather than "the tests":

- `tests/test_shims` — Mach ports (queueing, rights, port sets, timeouts)
  and the Darwin syscall shims
- `tests/test_xpc` — XPC dictionaries, wire framing, request/reply
  correlation, async delivery and error paths

Both assert on observable behaviour rather than return codes alone: a shim
that "succeeds" while losing bytes, dropping messages or mismatching replies
would pass a return-code-only suite.

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
