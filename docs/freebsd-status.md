# CatBSD FreeBSD Compatibility Status

Tracks which CatBSD components build and pass tests on FreeBSD.

**Last updated**: October 5, 2026 (macOS-only testing so far)  
**FreeBSD target**: 14.2+ / 14-STABLE / 15-CURRENT (amd64 + arm64/aarch64)

---

## Core Libraries

These are the foundation — they need to work before anything else matters.

| Component | macOS | FreeBSD amd64 | FreeBSD arm64 | Notes |
|-----------|-------|---------------|---------------|-------|
| `blocks/` — Blocks Runtime | ✅ Tests pass | ⬜ Untested | ⬜ Untested | MIT from LLVM compiler-rt; POSIX only |
| `shims/mach_port` — Mach port | ✅ Tests pass | ⬜ Untested | ⬜ Untested | Uses kqueue — native on FreeBSD |
| `shims/xpc_shim` — XPC | ✅ Tests pass | ⬜ Untested | ⬜ Untested | AF_UNIX only, pure POSIX |
| `libdispatch/` — GCD subset | ✅ Tests pass | ⬜ Untested | ⬜ Untested | pthreads + POSIX |
| `launchd/liblaunch` — supervision | ✅ Tests pass | ⬜ Untested | ⬜ Untested | POSIX only |
| `launchd/launchctl_server` — control | ✅ Tests pass | ⬜ Untested | ⬜ Untested | Depends on xpc_shim |

**Expected result**: All should build and pass on FreeBSD 14.x / 15.x with the stock
`clang` from base (no pkg installs needed). The Mach port shim uses `kqueue`
which is *native* to FreeBSD — it should work better there than on macOS.

---

## Binaries

| Binary | macOS | FreeBSD amd64 | FreeBSD arm64 | Notes |
|--------|-------|---------------|---------------|-------|
| `catbsd-init` | ✅ Smoke tests pass | ⬜ Untested | ⬜ Untested | PID 1 behaviour only testable in a VM |
| `catbsd-launchctl` | ✅ Builds, works | ⬜ Untested | ⬜ Untested | Thin XPC client |

---

## Essential 15 Utilities

The original demo utilities. Many wrap macOS-specific subsystems and will
need alternative backends on FreeBSD.

| Utility | macOS | FreeBSD | Backend needed on FreeBSD |
|---------|-------|---------|--------------------------|
| `sw_vers` | ✅ | 🔧 Needs port | Reads `/etc/os-release` or `uname -r` |
| `scutil` | ✅ | 🔧 Needs port | `sysctl` + `/etc/rc.conf` |
| `networksetup` | ✅ | 🔧 Needs port | `ifconfig` + `/etc/rc.conf` |
| `plutil` | ✅ | 🔧 Needs port | `libxml2` (pkg: `pkg install libxml2`) |
| `xattr` | ✅ | 🔧 Needs port | `extattr_*` syscalls (different API) |
| `ditto` | ✅ | 🔧 Needs port | `cpdup` or custom |
| `open` | ✅ | 🔧 Needs port | `xdg-open` (pkg: `xdg-utils`) |
| `pbcopy` | ✅ | ❌ No clipboard | `xclip`/`xsel` under X11 only |
| `pbpaste` | ✅ | ❌ No clipboard | Same |
| `say` | ✅ | 🔧 Needs port | `espeak` (pkg: `espeak`) |
| `afplay` | ✅ | 🔧 Needs port | `mpg123`/`ffplay` (pkg) |
| `screencapture` | ✅ | ❌ No Quartz | `scrot` under X11 only |
| `launchd` (demo) | ✅ | 🔧 Needs port | Use `catbsd-init` instead |
| `defaults` | ✅ | 🔧 Needs port | plist_lite already ported |
| `caffeinate` | ✅ | 🔧 Needs port | No direct equivalent; stub |

**Legend**: ✅ Works | 🔧 Needs FreeBSD backend | ❌ Fundamentally macOS-only

> [!NOTE]
> The Essential 15 are demos, not the core of the project. The core
> (liblaunch, XPC shim, libdispatch, catbsd-init) is the priority.

---

## Known Portability Considerations

### Things that should just work

- **kqueue** — native to FreeBSD; the Mach port shim was *designed* around it
- **pthreads** — in base (`libthr`), link with `-lpthread` on FreeBSD (auto in Makefile)
- **AF_UNIX sockets** — identical API on both platforms
- **`fork`/`exec`/`waitpid`** — standard POSIX, identical behaviour
- **`/proc/cmdline`** — FreeBSD mounts `procfs` at `/proc`; check `mount -t procfs proc /proc`

### Things that need attention

- **`__sync_bool_compare_and_swap`** — used in `blocks/config.h`; verified in
  `clang` from FreeBSD base. If using `gcc` from ports, test explicitly.
- **`clock_gettime(CLOCK_MONOTONIC)`** — standard on FreeBSD 14+, no issues expected
- **`SO_PEERCRED`** — called `LOCAL_PEERCRED` on FreeBSD/macOS; xpc_shim uses
  the correct portable path already
- **`pthread_setname_np`** — exists on FreeBSD 14 but takes 2 args (thread, name)
  vs macOS's 1 arg (name only); darwin_syscalls shim handles this

### Things that definitely won't work without porting

- `pbcopy`/`pbpaste` — macOS pasteboard API; need `xclip` on X11
- `screencapture` — macOS Quartz Compositor; need `scrot`/`spectacle` under X11
- `say` — macOS Speech Synthesis framework; need `espeak`/`festival`
- `afplay` — macOS CoreAudio; need `mpg123`/`ffplay`

---

## How to Update This File

Run the test suite on a real FreeBSD VM:

```bash
sh scripts/test-freebsd.sh
```

Then update the tables above with:
- ✅ Tests pass — component builds and all tests pass
- ⚠️ Builds, partial — compiles but some tests fail (note which)
- ❌ Does not build — compile error (link to issue or paste error)
- ⬜ Untested — nobody has tried yet

Include the FreeBSD version and architecture in your update.

---

## Test Run Log

| Date | Tester | OS | Arch | Result | Notes |
|------|--------|----|------|--------|-------|
| 2026-09-27 | Automated | macOS 27.2 Darwin | arm64 | ✅ 6/6 | Reference run — not FreeBSD |
| _(your run here)_ | | | | | |
