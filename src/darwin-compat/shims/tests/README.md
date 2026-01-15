# Darwin Compatibility Shim - Quick Start Demo

This is a practical demonstration showing how the CatBSD compatibility shims work.

## What This Demonstrates

The test program shows how Darwin/macOS code can run on FreeBSD using our compatibility layer:

1. **Mach Ports** → FreeBSD kqueue
2. **Darwin syscalls** → FreeBSD equivalents
3. **Real-world usage patterns**

## Running the Demo

```bash
cd src/darwin-compat/shims/tests
./run_tests.sh
```

## What You'll See

```
=== CatBSD Darwin Compatibility Shim Test ===

Testing Mach port compatibility...
  ✓ mach_port_allocate: port=3
  ✓ mach_msg_send: Success
  ✓ mach_msg_receive: Success
  ✓ mach_port_deallocate: success
  All Mach port tests passed!

Testing Darwin syscall compatibility...
  ✓ darwin_absolute_time: 10234567 ns elapsed
  ✓ darwin_thread_self: thread_id=34366234624
  ✓ darwin_pthread_setname_np: success
  ✓ darwin_vm_page_size: 4096 bytes
  All Darwin syscall tests passed!

Demonstrating practical usage...
  Processed 1000 messages in 1234 microseconds
  Average: 1234 ns per message

=== All tests passed! ===
```

## How It Works

### Mach Port Example

**Darwin code:**

```c
#include <mach/mach.h>

mach_port_t port;
mach_port_allocate(mach_task_self(), &port, MACH_PORT_RIGHT_RECEIVE);
mach_msg_send(port, msg, len);
```

**CatBSD equivalent (using shim):**

```c
#include "mach_port.h"  // Our shim

mach_port_t port;
mach_port_allocate(&port);  // Creates kqueue internally
mach_msg_send(port, msg, len);  // Uses kevent
```

### Darwin Syscall Example

**Darwin code:**

```c
#include <mach/mach_time.h>

uint64_t start = mach_absolute_time();
// ... work ...
uint64_t elapsed = mach_absolute_time() - start;
```

**CatBSD equivalent (using shim):**

```c
#include "darwin_syscalls.h"  // Our shim

uint64_t start = darwin_absolute_time();  // Uses clock_gettime
// ... work ...
uint64_t elapsed = darwin_absolute_time() - start;
```

## Key Insight

The beauty of this approach is that Darwin components can be ported with **minimal code changes**:

1. Change `#include <mach/mach.h>` to `#include "mach_port.h"`
2. The API stays the same
3. The shim handles FreeBSD translation

## Real-World Application

This is exactly how we'll port components like **launchd**:

```c
// launchd source code (simplified)
#ifdef FREEBSD_COMPAT
#include "mach_port.h"      // Use our shim
#else
#include <mach/mach.h>      // Use real Mach
#endif

void launchd_main(void) {
    mach_port_t port;
    mach_port_allocate(&port);  // Works on both!
    // ... rest of launchd code ...
}
```

## Next Steps

Try modifying the test program:

1. Add more message passing
2. Test error conditions
3. Benchmark performance
4. Add your own shim functions

This hands-on experience will help when porting real Darwin components!
