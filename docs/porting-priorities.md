# Darwin Component Porting Priorities

**CatBSD Phase 2 Analysis**  
**Updated**: January 15, 2026

## Overview

This document prioritizes Darwin components for porting to CatBSD based on:

- **Value**: Importance for macOS compatibility
- **Complexity**: Difficulty of porting
- **Dependencies**: What else is needed
- **Effort**: Estimated time to port

## Priority Matrix

```
High Value, Low Complexity → START HERE
High Value, High Complexity → Phase 4+
Low Value, Low Complexity → Nice to have
Low Value, High Complexity → Skip
```

## Phase 3: High Priority (Start Here)

### 1. Blocks Runtime ⭐⭐⭐

**Purpose**: Support for C blocks (closures)

**Why Important**:

- Required for libdispatch
- Required for modern Objective-C
- Foundation for GCD

**Complexity**: ⚡ Low  
**Effort**: 1-2 days  
**Dependencies**: None (Clang already supports syntax)

**Source**: LLVM compiler-rt  
**Status**: ✅ Already portable, well-tested

**Implementation**:

```bash
# Clone LLVM
git clone --depth 1 https://github.com/llvm/llvm-project.git
cp -R llvm-project/compiler-rt/lib/BlocksRuntime src/darwin-compat/blocks

# Build as shared library
cd src/darwin-compat/blocks
clang -shared -fPIC *.c -o libBlocksRuntime.so
```

**Success Criteria**:

- [ ] Compiles on FreeBSD
- [ ] Passes compiler-rt tests
- [ ] Simple block programs work

---

### 2. libdispatch (Grand Central Dispatch) ⭐⭐⭐

**Purpose**: Concurrent programming library

**Why Important**:

- Core to modern macOS development
- Used by many Darwin applications
- High-performance concurrency

**Complexity**: ⚡⚡ Medium  
**Effort**: 3-5 days  
**Dependencies**: Blocks runtime, pthreads, kqueue

**Source**: https://github.com/apple/swift-corelibs-libdispatch  
**Status**: ✅ Portable (already ported to Linux)

**Key Features**:

- Dispatch queues (serial, concurrent, global)
- Dispatch sources (timers, file descriptors, signals)
- Dispatch groups and semaphores
- Work items and barriers

**FreeBSD Compatibility**:

- ✅ kqueue (native FreeBSD support)
- ✅ pthreads (libthr)
- ✅ Blocks runtime (from Phase 3.1)
- ⚠️ May need minor patches for FreeBSD-specific APIs

**Implementation Plan**:

1. Clone swift-corelibs-libdispatch
2. Configure for FreeBSD (similar to Linux port)
3. Build with CMake
4. Run test suite
5. Fix FreeBSD-specific issues

**Success Criteria**:

- [ ] Compiles on FreeBSD
- [ ] Passes 90%+ of test suite
- [ ] Simple GCD programs work
- [ ] Performance is acceptable

---

### 3. CoreFoundation ⭐⭐⭐

**Purpose**: Core framework with fundamental data types

**Why Important**:

- Foundation for macOS applications
- Property list support
- Run loops and timers
- String and collection types

**Complexity**: ⚡⚡⚡ Medium-High  
**Effort**: 5-7 days  
**Dependencies**: libdispatch, ICU (Unicode), pthreads

**Source**: https://github.com/apple/swift-corelibs-foundation (CoreFoundation subset)  
**Status**: ⚠️ Partially portable

**Key Components**:

- CFString, CFArray, CFDictionary, CFSet
- CFRunLoop (event loop)
- CFPropertyList (plist files)
- CFBundle (application bundles)
- CFPreferences (user defaults)

**Porting Challenges**:

- Some macOS-specific features (CFBundle may need work)
- Run loop integration with kqueue
- ICU dependency (large library)

**Implementation Plan**:

1. Install ICU library on FreeBSD
2. Clone swift-corelibs-foundation
3. Build CoreFoundation subset
4. Stub out macOS-specific features
5. Test with simple CF programs

**Success Criteria**:

- [ ] Core data types work (CFString, CFArray, etc.)
- [ ] Property lists can be read/written
- [ ] Run loops function correctly
- [ ] Basic CF programs compile and run

---

### 4. Libsystem Compatibility Layer ⭐⭐⭐

**Purpose**: Darwin system library interface

**Why Important**:

- Required for Darwin binary compatibility
- Maps Darwin APIs to FreeBSD
- Provides Darwin-specific functions

**Complexity**: ⚡⚡ Medium  
**Effort**: 3-5 days  
**Dependencies**: All above components

**Components to Shim**:

- `libsystem_c` → FreeBSD libc + extensions
- `libsystem_pthread` → FreeBSD libthr + extensions
- `libsystem_kernel` → Syscall wrappers
- `libsystem_malloc` → FreeBSD malloc
- `libsystem_blocks` → Blocks runtime
- `libdispatch` → GCD

**Implementation Strategy**:

```
src/darwin-compat/libsystem/
├── include/           # Darwin-compatible headers
├── libc_shims.c       # C library extensions
├── pthread_shims.c    # Threading extensions
├── kernel_shims.c     # Syscall wrappers
└── Makefile
```

**Key Functions to Implement**:

- `copyfile()`, `fcopyfile()`, `clonefile()`
- `fgetattrlist()`, `fsetattrlist()`
- `getiopolicy_np()`, `setiopolicy_np()`
- `bsdthread_*()` → map to pthread
- `workq_*()` → map to GCD

**Success Criteria**:

- [ ] Darwin headers compile on FreeBSD
- [ ] Common Darwin functions work
- [ ] Simple Darwin CLI tools run

## Phase 4: Medium Priority

### 5. launchd ⭐⭐

**Purpose**: Init system and service manager

**Why Important**:

- macOS-style service management
- On-demand launching
- Socket activation

**Complexity**: ⚡⚡⚡⚡ High  
**Effort**: 7-14 days  
**Dependencies**: XPC (Mach IPC), Libsystem, CoreFoundation

**Challenges**:

- Heavily depends on XPC (Mach IPC)
- Tight integration with macOS kernel
- Complex codebase

**Alternative Approaches**:

**Option 1**: Full launchd port

- Port XPC as well (very complex)
- Emulate Mach IPC with sockets
- High effort, high compatibility

**Option 2**: Simplified launchd

- Implement plist parsing
- Basic service supervision
- Map to FreeBSD rc.d underneath
- Medium effort, medium compatibility

**Option 3**: launchd compatibility wrapper

- Parse launchd plists
- Convert to rc.d scripts
- Low effort, basic compatibility

**Recommendation**: Start with Option 3, evaluate Option 2 later

**Success Criteria**:

- [ ] Can parse launchd plists
- [ ] Can launch services
- [ ] Basic supervision works

---

### 6. Darwin libc Extensions ⭐⭐

**Purpose**: Darwin-specific C library functions

**Complexity**: ⚡⚡ Medium  
**Effort**: 2-4 days (incremental)  
**Dependencies**: FreeBSD libc

**Functions to Implement**:

**File Operations**:

- `copyfile()` - Efficient file copying with metadata
- `fcopyfile()` - File descriptor version
- `clonefile()` - Copy-on-write cloning (APFS feature)

**Extended Attributes**:

- `fgetattrlist()` - Get file attributes
- `fsetattrlist()` - Set file attributes
- `getattrlist()`, `setattrlist()` - Path versions

**I/O Policy**:

- `getiopolicy_np()` - Get I/O policy
- `setiopolicy_np()` - Set I/O policy (throttling, etc.)

**Implementation**: Add to libsystem compatibility layer

## Phase 5+: Low Priority

### 7. Additional Frameworks ⭐

**Candidates**:

- Security framework (crypto, keychain)
- CFNetwork (networking)
- Foundation (requires Objective-C runtime)

**Approach**: Add as needed based on application requirements

### 8. Objective-C Runtime ⭐

**Purpose**: Support for Objective-C applications

**Complexity**: ⚡⚡⚡ Medium-High  
**Effort**: Variable

**Options**:

- Use GNUstep Objective-C runtime
- Port Apple's objc4 runtime
- Defer until needed

### 9. Darwin Command-Line Tools ⭐

**Examples**:

- `defaults` - Property list editor
- `plutil` - Plist utility
- `launchctl` - launchd control

**Approach**: Port individually as needed

## Not Recommended

### ❌ XNU Kernel Components

**Why Skip**:

- FreeBSD kernel is excellent
- XNU integration is extremely complex
- Low value for effort

### ❌ IOKit

**Why Skip**:

- C++ in kernel (FreeBSD avoids this)
- Architectural mismatch
- FreeBSD driver model is sufficient

### ❌ Mach IPC (in kernel)

**Why Skip**:

- Core XNU feature, can't extract easily
- Can emulate in userland if needed
- Most apps don't use directly

## Implementation Roadmap

### Week 1-2: Foundation

- [x] Phase 2 analysis (current)
- [ ] Set up darwin-compat structure
- [ ] Port Blocks runtime
- [ ] Verify Blocks work

### Week 3-4: Concurrency

- [ ] Port libdispatch
- [ ] Run GCD tests
- [ ] Create simple GCD examples
- [ ] Performance testing

### Week 5-6: Core Framework

- [ ] Install ICU dependency
- [ ] Port CoreFoundation
- [ ] Test property lists
- [ ] Test run loops

### Week 7-8: Compatibility

- [ ] Build Libsystem shims
- [ ] Implement Darwin libc extensions
- [ ] Test Darwin CLI tools
- [ ] Integration testing

### Week 9+: Services (Optional)

- [ ] Evaluate launchd approaches
- [ ] Implement chosen approach
- [ ] Test service management

## Success Metrics

### Phase 3 Complete When:

- [ ] Blocks runtime compiles and works
- [ ] libdispatch passes 90%+ tests
- [ ] CoreFoundation core features work
- [ ] Simple Darwin apps compile and run
- [ ] Property lists can be read/written

### Phase 4 Complete When:

- [ ] launchd plists can be parsed
- [ ] Services can be launched
- [ ] Darwin CLI tools work
- [ ] Integration is stable

## Dependencies Graph

```
Blocks Runtime (no deps)
    ↓
libdispatch (needs Blocks, pthreads, kqueue)
    ↓
CoreFoundation (needs libdispatch, ICU)
    ↓
Libsystem (needs all above)
    ↓
launchd (needs Libsystem, CoreFoundation)
```

## Effort Summary

| Component         | Complexity  | Days           | Priority |
| ----------------- | ----------- | -------------- | -------- |
| Blocks Runtime    | Low         | 1-2            | HIGH     |
| libdispatch       | Medium      | 3-5            | HIGH     |
| CoreFoundation    | Medium-High | 5-7            | HIGH     |
| Libsystem         | Medium      | 3-5            | HIGH     |
| **Phase 3 Total** | -           | **12-19 days** | -        |
| launchd           | High        | 7-14           | MEDIUM   |
| libc Extensions   | Medium      | 2-4            | MEDIUM   |
| **Phase 4 Total** | -           | **9-18 days**  | -        |

**Total Estimated Effort**: 3-5 weeks for core compatibility

## References

- swift-corelibs-libdispatch: https://github.com/apple/swift-corelibs-libdispatch
- swift-corelibs-foundation: https://github.com/apple/swift-corelibs-foundation
- LLVM compiler-rt (Blocks): https://github.com/llvm/llvm-project
- ravynOS porting experience
- GNUstep project (Objective-C runtime)
