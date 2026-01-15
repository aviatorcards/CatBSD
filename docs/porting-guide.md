# CatBSD Porting Guide

Guide for porting Darwin/macOS components to CatBSD.

## Overview

This guide covers the process of importing and adapting Darwin components to work with the FreeBSD-based CatBSD system.

## Porting Strategy

### 1. Component Selection

**Criteria for porting**:

- ✅ Open source (APSL licensed)
- ✅ Minimal kernel dependencies
- ✅ Clear benefit to CatBSD
- ✅ Reasonable complexity
- ❌ Proprietary or closed-source
- ❌ Heavily macOS-specific
- ❌ Requires extensive XNU kernel features

### 2. Difficulty Assessment

| Component      | Difficulty  | Dependencies             | Priority | Effort          |
| -------------- | ----------- | ------------------------ | -------- | --------------- |
| Blocks Runtime | Low         | None (Clang)             | High     | 1-2 days        |
| libdispatch    | Medium      | Blocks, pthreads, kqueue | High     | 3-5 days        |
| CoreFoundation | Medium-High | libdispatch, ICU         | High     | 5-7 days        |
| Libsystem      | Medium      | All above                | High     | 3-5 days        |
| launchd        | High        | XPC, Libsystem           | Medium   | 7-14 days       |
| IOKit          | Very High   | XNU kernel               | Low      | Not recommended |

See [porting-priorities.md](porting-priorities.md) for detailed analysis.

## Porting Process

### Step 1: Extract Source

```bash
# Navigate to Apple OSS
cd src/apple-oss

# Find component (example: launchd)
ls -la | grep -i launchd

# Copy to darwin-compat
cp -r launchd ../darwin-compat/
```

### Step 2: Analyze Dependencies

```bash
cd ../darwin-compat/launchd

# Find includes
grep -r "#include" . | grep -v ".git" > includes.txt

# Identify system calls
grep -r "syscall\|mach_" . > syscalls.txt

# Check for kernel dependencies
grep -r "kernel\|xnu" . > kernel_deps.txt
```

### Step 3: Create Compatibility Shims

**Example**: Mach port shim for launchd

```c
// src/darwin-compat/shims/mach_port.h
#ifndef _MACH_PORT_SHIM_H_
#define _MACH_PORT_SHIM_H_

#include <sys/types.h>
#include <sys/event.h>

// Map Mach ports to FreeBSD kqueue
typedef int mach_port_t;

// Shim functions
mach_port_t mach_port_allocate(void);
int mach_port_deallocate(mach_port_t port);
int mach_msg_send(mach_port_t port, void *msg, size_t len);
int mach_msg_receive(mach_port_t port, void *msg, size_t len);

#endif /* _MACH_PORT_SHIM_H_ */
```

```c
// src/darwin-compat/shims/mach_port.c
#include "mach_port.h"
#include <sys/event.h>
#include <unistd.h>

mach_port_t mach_port_allocate(void) {
    // Create kqueue as Mach port equivalent
    return kqueue();
}

int mach_port_deallocate(mach_port_t port) {
    return close(port);
}

// Implement send/receive using kevent...
```

### Step 4: Modify Build System

**Create component Makefile**:

```makefile
# src/darwin-compat/launchd/Makefile.bsd
# FreeBSD-compatible Makefile for launchd

PROG=   launchd
SRCS=   launchd.c job.c protocol.c
SRCS+=  ../shims/mach_port.c

CFLAGS+= -I../shims
CFLAGS+= -DFREEBSD_COMPAT

LDADD=  -lpthread -lutil

.include <bsd.prog.mk>
```

### Step 5: Patch Source Code

**Create patch file**:

```bash
# Make changes to source
cd src/darwin-compat/launchd
# ... edit files ...

# Generate patch
diff -Naur original/ modified/ > ../../patches/darwin/launchd-freebsd.patch
```

**Example patch**:

```diff
--- a/launchd.c
+++ b/launchd.c
@@ -1,7 +1,12 @@
 #include <stdio.h>
-#include <mach/mach.h>
+#ifdef FREEBSD_COMPAT
+#include "mach_port.h"
+#else
+#include <mach/mach.h>
+#endif

 int main(int argc, char *argv[]) {
+    printf("CatBSD launchd starting...\n");
     // ... rest of code
 }
```

### Step 6: Build and Test

```bash
# Build component
cd src/darwin-compat/launchd
make -f Makefile.bsd

# Test in isolation
./launchd --help

# Install to test system
sudo make install
```

## Common Porting Patterns

### Pattern 1: Syscall Translation

**Darwin syscall → FreeBSD equivalent**

```c
// Darwin: mach_absolute_time()
// FreeBSD: clock_gettime()

#ifdef FREEBSD_COMPAT
uint64_t mach_absolute_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif
```

### Pattern 2: Header Adaptation

```c
// Create compatibility header
// src/darwin-compat/shims/darwin_compat.h

#ifdef FREEBSD_COMPAT
#include <sys/types.h>
#include <sys/event.h>

// Map Darwin types to FreeBSD
typedef struct kevent mach_msg_header_t;
typedef int kern_return_t;

#define KERN_SUCCESS 0
#define KERN_FAILURE -1

#endif
```

### Pattern 3: Library Linking

```makefile
# Link against FreeBSD equivalents
# Darwin: -framework CoreFoundation
# FreeBSD: -lcf (our ported version)

LDADD= -lcf -ldispatch -lsystem
```

## Component-Specific Guides

### launchd

**Challenges**:

- Mach port IPC → kqueue
- XPC → Unix domain sockets
- Sandbox integration → Capsicum

**Approach**:

1. Start with basic daemon management
2. Add IPC layer using kqueue
3. Integrate with FreeBSD rc.d for compatibility

### libdispatch (GCD)

**Challenges**:

- Workqueue syscalls → pthread pools
- Mach semaphores → POSIX semaphores

**Approach**:

1. Use existing FreeBSD port as reference
2. Adapt to CatBSD environment
3. Test with simple dispatch programs

### CoreFoundation

**Challenges**:

- Large dependency tree
- Many internal APIs
- ICU integration

**Approach**:

1. Port minimal subset first
2. Use FreeBSD's libiconv
3. Stub out macOS-specific features

## Testing Strategy

### Unit Tests

```bash
# Create test suite
mkdir src/darwin-compat/tests

# Example test
cat > src/darwin-compat/tests/test_launchd.c << 'EOF'
#include <assert.h>
#include "launchd.h"

int main() {
    // Test basic functionality
    assert(launchd_init() == 0);
    assert(launchd_load_job("test.plist") == 0);
    return 0;
}
EOF
```

### Integration Tests

```bash
# Test in CatBSD VM
# 1. Boot CatBSD
# 2. Load ported component
# 3. Verify functionality
# 4. Check logs for errors
```

## Troubleshooting

### Linker Errors

**Problem**: Undefined symbols

**Solution**:

```bash
# Find missing symbols
nm -u launchd | grep "U "

# Implement shims or link libraries
```

### Runtime Crashes

**Problem**: Segfaults or assertion failures

**Solution**:

```bash
# Debug with lldb
lldb launchd
(lldb) run
(lldb) bt  # backtrace on crash
```

### API Mismatches

**Problem**: Function signatures don't match

**Solution**: Create adapter functions

```c
// Adapter for different signatures
int darwin_api(int x, int y) {
    return freebsd_api(x, y, 0);  // Add default param
}
```

## Best Practices

1. **Document Everything**: Comment why changes were made
2. **Keep Patches Small**: One logical change per patch
3. **Test Incrementally**: Build and test frequently
4. **Maintain Compatibility**: Support both Darwin and FreeBSD when possible
5. **Version Control**: Commit often with clear messages

## Resources

- [Apple Open Source](https://opensource.apple.com/)
- [FreeBSD Porter's Handbook](https://docs.freebsd.org/en/books/porters-handbook/)
- [Darwin Source Browser](https://github.com/apple-oss-distributions)
- [ravynOS Porting Notes](https://github.com/ravynsoft/ravynos)

## Next Steps

After porting a component:

1. Update `SOURCE_INVENTORY.md`
2. Document in `docs/ported-components.md`
3. Add to build system
4. Create test cases
5. Update architecture docs
