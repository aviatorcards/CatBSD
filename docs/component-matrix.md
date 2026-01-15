# CatBSD Component Porting Matrix

Comprehensive analysis of Darwin components for porting to CatBSD.

## Component Priority Matrix

| Component          | LOC        | Dependencies    | Kernel APIs    | Difficulty | Priority | Estimated Effort | Status                  |
| ------------------ | ---------- | --------------- | -------------- | ---------- | -------- | ---------------- | ----------------------- |
| **libdispatch**    | ~30k       | pthread, kqueue | Minimal        | Medium     | **High** | 1-2 weeks        | ✅ Ready to port        |
| **launchd**        | ~50k       | libsystem, XPC  | Mach ports     | Medium     | **High** | 2-3 weeks        | Ready after libdispatch |
| **CoreFoundation** | ~100k      | ICU, libiconv   | Minimal        | High       | **High** | 4-6 weeks        | Needs libdispatch       |
| **Libsystem**      | Varies     | Kernel          | Many syscalls  | High       | Medium   | 3-4 weeks        | Complex integration     |
| **dyld**           | ~40k       | Kernel          | Mach-O loading | Very High  | Low      | 6-8 weeks        | Not priority            |
| **Security**       | Large      | CF, IOKit       | Many           | Very High  | Medium   | 6-8 weeks        | Future                  |
| **IOKit**          | Very Large | XNU kernel      | Deep kernel    | Very High  | Low      | Months           | Likely infeasible       |

## Recommended Porting Order

### Phase 3: Initial Integration (Current)

#### 1. libdispatch (Grand Central Dispatch) ⭐ START HERE

**Why first:**

- ✅ Already has FreeBSD port history (Swift uses it)
- ✅ Minimal kernel dependencies
- ✅ Uses kqueue (native FreeBSD!)
- ✅ High value - many components depend on it
- ✅ Medium difficulty - good learning experience

**Dependencies:**

- pthread (✅ available)
- kqueue (✅ native FreeBSD)
- Minimal syscalls

**Approach:**

1. Extract from Apple OSS
2. Use existing FreeBSD port as reference
3. Adapt build system
4. Test with simple dispatch programs

**Timeline:** 1-2 weeks

---

#### 2. launchd (Init System)

**Why second:**

- Depends on libdispatch (port first)
- High value for CatBSD
- Replaces rc.d with modern init

**Dependencies:**

- libdispatch (✅ after step 1)
- XPC (needs shim)
- Mach ports (✅ have shim)

**Approach:**

1. Port basic daemon management
2. Implement XPC shim
3. Integrate with FreeBSD rc.d for compatibility

**Timeline:** 2-3 weeks

---

### Phase 4: Enhanced Userland

#### 3. CoreFoundation

**Why third:**

- Foundation for many frameworks
- Needed for advanced features
- Large but well-documented

**Dependencies:**

- libdispatch (✅ after step 1)
- ICU (available via ports)
- libiconv (✅ FreeBSD has it)

**Approach:**

1. Port minimal subset first
2. Gradually add components
3. Stub out macOS-specific features

**Timeline:** 4-6 weeks

---

#### 4. Libsystem Components

**As needed:**

- Port individual components
- Focus on what's needed for other ports
- Incremental approach

---

## Detailed Component Analysis

### libdispatch (Grand Central Dispatch)

**Purpose:** Concurrent programming framework

**Key Features:**

- Dispatch queues (serial, concurrent)
- Dispatch sources (timers, file descriptors)
- Dispatch groups (task coordination)
- Dispatch semaphores

**FreeBSD Compatibility:**

- ✅ Uses kqueue (native!)
- ✅ Uses pthread
- ⚠️ Workqueue syscalls (can use pthread pools)
- ✅ No Mach dependencies

**Porting Strategy:**

1. Start with existing Swift/FreeBSD port
2. Adapt for CatBSD environment
3. Test with example programs
4. Benchmark performance

**Test Plan:**

```c
// Simple dispatch test
dispatch_queue_t queue = dispatch_queue_create("test", NULL);
dispatch_async(queue, ^{
    printf("Hello from GCD!\n");
});
dispatch_release(queue);
```

---

### launchd

**Purpose:** Service management and init system

**Key Features:**

- On-demand service activation
- Socket activation
- Dependency management
- Process monitoring

**FreeBSD Compatibility:**

- ⚠️ Uses Mach ports (✅ have shim)
- ⚠️ Uses XPC (need shim)
- ✅ Process management (standard POSIX)

**Porting Strategy:**

1. Basic daemon management first
2. Add IPC layer (Mach port shim)
3. Implement XPC shim
4. Integrate with rc.d

**Integration Approach:**

- Keep FreeBSD rc.d for compatibility
- Add launchd as alternative
- Support both .plist and rc.d scripts

---

### CoreFoundation

**Purpose:** Core data structures and utilities

**Key Components:**

- CFString, CFArray, CFDictionary
- CFRunLoop (event loop)
- CFBundle (plugin loading)
- Property lists

**FreeBSD Compatibility:**

- ✅ Mostly userland code
- ⚠️ Some macOS-specific features
- ✅ ICU available

**Porting Strategy:**

1. Port core data structures first
2. Add CFRunLoop (uses kqueue!)
3. Stub out macOS-specific features
4. Incremental testing

---

## Component Dependencies Graph

```
CoreFoundation
    ↓
libdispatch ← launchd
    ↓           ↓
  pthread    Mach ports (shim)
    ↓           ↓
  FreeBSD   kqueue
```

## Porting Workflow (Proven Pattern)

Based on our successful shim library test:

1. **Extract** - Use `extract-darwin-component.sh`
2. **Analyze** - Review dependencies and APIs
3. **Shim** - Create/update compatibility shims
4. **Build** - Adapt Makefile for FreeBSD
5. **Test** - Create test programs
6. **Iterate** - Fix issues, optimize
7. **Document** - Update docs and checklists

## Success Criteria

### libdispatch

- [ ] Builds successfully
- [ ] Basic dispatch_async works
- [ ] Dispatch queues functional
- [ ] Dispatch sources work
- [ ] Performance acceptable

### launchd

- [ ] Starts and manages daemons
- [ ] Socket activation works
- [ ] IPC functional
- [ ] Integrates with system

### CoreFoundation

- [ ] Core data structures work
- [ ] CFRunLoop functional
- [ ] Property lists work
- [ ] Compatible with Darwin apps

## Risk Assessment

### Low Risk

- libdispatch (proven FreeBSD compatibility)
- Basic data structures

### Medium Risk

- launchd (IPC complexity)
- CoreFoundation (size and complexity)

### High Risk

- Libsystem (deep kernel integration)
- IOKit (requires XNU kernel)

## Resources

### Existing Ports

- [Swift libdispatch](https://github.com/apple/swift-corelibs-libdispatch) - FreeBSD support
- [GNUstep CoreFoundation](https://github.com/gnustep/libs-corebase) - Alternative implementation
- [ravynOS](https://github.com/ravynsoft/ravynos) - Similar project

### Documentation

- [Apple Open Source](https://opensource.apple.com/)
- [libdispatch Documentation](https://developer.apple.com/documentation/dispatch)
- [launchd.plist Manual](https://www.manpagez.com/man/5/launchd.plist/)

## Next Immediate Steps

1. ✅ Shim library working
2. 🔄 Create component matrix (this document)
3. ⏭️ Fetch Apple OSS sources
4. ⏭️ Extract libdispatch
5. ⏭️ Begin libdispatch port

---

**Last Updated:** 2026-01-15  
**Status:** Ready to begin Phase 3 with libdispatch
