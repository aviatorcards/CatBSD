# RFC & Roadmap: CatBSD as a FreeBSD Derivative / Fork

**Status**: Proposal / Architectural Discussion  
**Date**: October 2026  
**Target Upstream Baseline**: FreeBSD 14-STABLE / 15-CURRENT  

---

## 1. Executive Summary

CatBSD began as an exploratory compatibility layer: proving that key Darwin paradigms (launchd, Grand Central Dispatch/libdispatch, Blocks runtime, Mach port emulation over kqueue, and XPC IPC) could run on FreeBSD.

With **Milestone 1** (`launchctl` over XPC), **Milestone 2** (`catbsd-init` PID 1 skeleton), and **Milestone 3** (Blocks Runtime) completed and verified, CatBSD faces a strategic fork in the road:

1. **Option A: Remain an add-on package / userland overlay** running on stock FreeBSD.
2. **Option B: Evolve into a downstream FreeBSD distribution / fork** with its own identity, boot media, and default service architecture.

This document outlines why Option B is technically compelling, how historical precedent guides it, and how to execute it cleanly without drowning in kernel maintenance.

---

## 2. Historical Precedent

The idea of blending Darwin and BSD into a dedicated operating system has direct historical roots:

* **NeXTSTEP (1989)**: Blended 4.3BSD/4.4BSD userland with the Mach 2.5/3.0 microkernel and Objective-C runtime.
* **Apple Darwin / OS X (2000–present)**: Combined Mach 3.0 IPC, FreeBSD 4/5 VFS and network stack, Apple DriverKit/IOKit, and eventually replaced traditional Unix init with `launchd` in Mac OS X 10.4 Tiger (2005).
* **FreeBSD Derivatives (GhostBSD, MidnightBSD, TrueOS/pfSense)**: Proved that downstream FreeBSD distributions can successfully modernize desktop or appliance experiences while keeping the robust FreeBSD kernel and ZFS foundation intact.

CatBSD represents the reciprocal vision: **a modern FreeBSD foundation whose userland and init architecture adopt the elegance of Darwin.**

---

## 3. Why an Add-on Layer Hits a Ceiling

If CatBSD remains merely a collection of utilities installed into `/usr/local/bin`:

1. **The Init System Dilemma**:
   FreeBSD's `/sbin/init` expects `/etc/rc` and shell-based runscripts in `/etc/rc.d/`. Overriding this requires manual bootloader intervention (`init_path="/sbin/catbsd-init"` in `/boot/loader.conf`). A broken update leaves the system unbootable.
2. **Dual-Stack Service Conflict**:
   Having both classic `/etc/rc.d` daemons and CatBSD plist jobs fighting for syslog, network interfaces, and consoles causes confusion and service collisions.
3. **Library and Header Placement**:
   Core primitives like `libdispatch.so` and `libBlocksRuntime.so` belong in `/lib` and `/usr/lib`, linked transparently by base system utilities, not segregated into custom include paths.

Becoming a **downstream distribution** solves these issues by establishing clear authority over the boot path and root filesystem layout.

---

## 4. The Three-Phase Evolution Strategy

```
┌────────────────────────────────────────────────────────┐
│ Phase 1: Userland Compatibility Layer (Current)        │
│ • Standalone libraries (blocks, shims, libdispatch)   │
│ • catbsd-init tested as daemon / alternate init        │
│ • Verified locally on macOS & inside FreeBSD VM        │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 2: FreeBSD PkgBase Distribution Overlay          │
│ • Leverage FreeBSD 14/15 pkgbase architecture          │
│ • Custom repo providing catbsd-init, catbsd-launchctl  │
│ • Replaces base-init cleanly via package manager       │
└──────────────────────────┬─────────────────────────────┘
                           │
                           ▼
┌────────────────────────────────────────────────────────┐
│ Phase 3: Standalone CatBSD Distribution / Fork         │
│ • Tracking fork of freebsd-src (stable/14 or 15)       │
│ • catbsd-init built directly into /sbin/init           │
│ • Custom bootable ISOs / VM disk images                │
│ • Default launchd plist service architecture           │
└────────────────────────────────────────────────────────┘
```

### Phase 1: Modular Userland (Where We Are Now)
* Keep all CatBSD components portable and building via `scripts/test-freebsd.sh`.
* Validate on both Apple Silicon (arm64) and Intel (amd64) FreeBSD VMs.
* Exercise `catbsd-init` under controlled supervisor conditions.

### Phase 2: PkgBase Overlay (Low Friction)
* FreeBSD 14 and 15 package the entire base system into `pkg` packages (`FreeBSD-kernel`, `FreeBSD-runtime`, `FreeBSD-rc`, etc.).
* CatBSD packages its components as overlay packages:
  * `catbsd-init` (provides alternative PID 1)
  * `catbsd-launchd-compat`
  * `catbsd-blocks`
  * `catbsd-daemons` (default plists for syslog, getty, devd)
* Users or developers can turn a stock FreeBSD installation into a CatBSD system with a single `pkg install catbsd-base`.

### Phase 3: Full Downstream Fork & Release Media
* Fork `freebsd-src` on GitHub (tracking `stable/14` or `main` for 15-CURRENT).
* Integrate `src/catbsd-init` into FreeBSD's `Makefile.inc1` and build system.
* Use FreeBSD's `release/release.sh` to generate:
  * `CatBSD-14.x-arm64.iso`
  * `CatBSD-14.x-amd64.iso`
  * Cloud / UTM ready-to-boot disk images (`.raw.xz` / `.qcow2.xz`).
* First boot takes the user directly to a CatBSD branded login console powered by `catbsd-init`.

---

## 5. Kernel Strategy: Upstream-First Tracking

A critical mistake of many historical OS forks is attempting to heavily modify the kernel from day one, creating an insurmountable merge burden when upstream releases new versions.

**CatBSD's kernel rule:**
> **The FreeBSD kernel remains 99% stock.**

* **Hardware & Drivers**: Track upstream FreeBSD. Let FreeBSD maintainers handle Wi-Fi, Ethernet, NVMe, USB, and ARM SoC support.
* **Storage & Security**: Keep ZFS, UFS2, GEOM, Capsicum, and pf untouched.
* **Kernel Shims (Optional / Future)**:
  If Darwin binary compatibility (running Mach-O binaries or Darwin syscalls directly) is ever explored in the future, it would be implemented as a clean kernel module (`sys/compat/darwin/`), mirroring FreeBSD's existing `compat_linux` infrastructure.

---

## 6. Target Baseline Recommendation

| Baseline | Recommendation | Rationale |
|---|---|---|
| **FreeBSD 13.x** | ❌ Deprecated | EOL approaching; older Clang; poor Apple Silicon virtualization support. |
| **FreeBSD 14-STABLE / 14.2+** | ✅ **Recommended Primary** | Rock-solid, official binary packages, mature arm64 Hypervisor support, stable base. |
| **FreeBSD 15-CURRENT** | 🧪 **Experimental Track** | Clang 18/19+, latest compiler-rt, active PkgBase modernization, longest runway. |

**Immediate Action for CatBSD development:**
Use **FreeBSD 14.2-RELEASE or 14-STABLE** (or latest snapshot) as the testbed today in UTM. As FreeBSD 15 nears release, evaluate rebasing the primary branch onto 15.

---

## 7. Decision Gates

Before committing to maintaining a public OS fork and publishing boot ISOs:

1. **Gate 1: VM Verification**
   Run `./scripts/test-freebsd.sh` on a live FreeBSD 14.x arm64 VM. Confirm 6/6 tests pass without modification.
2. **Gate 2: Live PID 1 Boot**
   Test booting a FreeBSD VM directly with `init_path="/sbin/catbsd-init"`. Confirm console getty respawns and system boots without kernel panic.
3. **Gate 3: Core Daemon Coverage**
   Provide launchd plists for the minimum viable base system:
   * Console getty (`com.catbsd.getty.plist`)
   * Device event daemon (`com.catbsd.devd.plist`)
   * Network configuration / DHCP (`com.catbsd.network.plist`)
   * Syslog (`com.catbsd.syslog.plist`)
4. **Gate 4: Build System Integration**
   Add a Makefile target or script that produces a bootable raw disk image (`make catbsd-image`).

Once Gate 2 and 3 are proven, the decision to publish an official CatBSD fork and installation ISOs becomes straightforward and low-risk.
