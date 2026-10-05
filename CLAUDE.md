# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo actually is right now

CatBSD's long-term goal (see `PLAN.md` and `docs/architecture.md`) is a hybrid OS: FreeBSD kernel/userland + ported Darwin components (launchd, libdispatch, CoreFoundation, etc.), eventually bootable. **That goal is not implemented yet.** `src/freebsd/` and `src/apple-oss/` are empty — the source-fetch/import pipeline described in the docs has not been run in this checkout.

What actually exists and works today is `src/darwin-compat/`: 15 small, self-contained C demo programs ("The Essential 15") that reimplement Darwin/macOS CLI tool *interfaces* (not ported Apple source), plus three support libraries — the compatibility shims (`shims/`), a libdispatch subset (`libdispatch/`) and a launchd job-supervision library (`launchd/`). All of it is clean-room CatBSD code, not ported Apple source. Treat the rest of the tree (`src/kernel/`, `src/patches/`, `scripts/*.sh` for fetching/importing FreeBSD+Apple sources) as scaffolding for future phases, not working infrastructure — several of these scripts assume tools like `darwinbuild` and multi-GB source trees that aren't present here.

Two of the three libraries are POSIX-only and build (and are tested) on Linux as well as macOS/FreeBSD; `shims/mach_port.c` and `shims/darwin_syscalls.c` use kqueue and BSD `sysctl`, so they are macOS/FreeBSD-only. `libdispatch/` and `launchd/` have been exercised under TSan/ASan on Linux but **have not yet been run on macOS or FreeBSD** — if something there misbehaves, that's why.

When asked to "build CatBSD" or "run the tests," it almost always means working within `src/darwin-compat/`.

## Common commands

All from `src/darwin-compat/`:

```bash
# Build everything: the 3 support libraries, then all 15 utilities
./build-all.sh
./build-all.sh clean
./build-all.sh test    # runs every library's test suite

# Build a single utility
cd <name>-demo && make            # produces ./<name>-demo binary
cd <name>-demo && make test       # most demos have a `make test` target
cd <name>-demo && make clean

# Build/test a support library alone
cd shims && make && make test        # libdarwin_compat.a; test_shims + test_xpc
cd libdispatch && make && make test  # libcatbsd_dispatch.a
cd launchd && make && make test      # liblaunch.a

# Run the showcase demos
./demo.sh              # original 5-utility demo
./demo-essential-15.sh # full 15-utility demo
```

`build-all.sh` covers all 15 utilities and all 3 libraries. There is no top-level build system, CI config, or linter in this repo — each library and utility is an independent Makefile-based C project sharing one BSD-style Makefile pattern.

## Architecture of `src/darwin-compat/`

```
darwin-compat/
├── shims/                  # libdarwin_compat.a
│   ├── mach_port.{c,h}     # Mach port IPC emulated via kqueue (BSD-only)
│   ├── darwin_syscalls.{c,h}  # (BSD-only: sysctl, pthread_np)
│   ├── xpc_shim.{c,h}      # XPC over AF_UNIX (portable)
│   ├── xpc_compat.h        # Darwin-name macro aliases, non-Apple only
│   └── tests/              # test_shims, test_xpc
├── libdispatch/            # libcatbsd_dispatch.a (portable)
│   ├── dispatch_shim.{c,h} # GCD subset on a shared pthread worker pool
│   ├── dispatch_compat.h   # Darwin-name macro aliases, non-Apple only
│   └── tests/
├── launchd/                # liblaunch.a (portable)
│   ├── plist_lite.{c,h}    # XML plist reader, no libxml2
│   ├── launch_job.{c,h}    # job model, registry, supervision
│   └── tests/
├── <name>-demo/            # one directory per utility, each independent
│   ├── <name>-demo.c
│   ├── Makefile
│   └── (test fixtures, e.g. demo.plist)
├── build-all.sh
├── demo.sh / demo-essential-15.sh
└── MANIFEST.md             # imported vs. clean-room component tracking
```

Every Makefile here follows the same BSD-style pattern (`CC`, `CFLAGS`, `SRCS`, `.c.o` suffix rule, `all`/`clean`/`test` targets) — copy it when adding a component rather than inventing a new build style. The three library Makefiles additionally define `-DFREEBSD_COMPAT` and build a static archive (`ar`/`ranlib`).

**Symbol naming convention, and why it matters:** anything that duplicates an API macOS already provides is prefixed `catbsd_` (`catbsd_xpc_*`, `catbsd_dispatch_*`, `CATBSD_PORT_RIGHT_*`). Declaring `xpc_object_t`, `dispatch_queue_t` or `MACH_PORT_RIGHT_SEND` with our own definitions is a hard compile error the moment a system header lands in the same translation unit — `mach_port.h` has a comment documenting exactly that bite. Each such component ships a `*_compat.h` that macro-maps the Darwin spelling onto the prefixed symbols and `#error`s on `__APPLE__`. Follow this when adding shims.

The 15 utilities are grouped by theme (see README.md): system/config (`sw_vers`, `scutil`, `networksetup`), file ops (`plutil`, `xattr`, `ditto`, `open`), clipboard (`pbcopy`, `pbpaste`), media (`say`, `afplay`, `screencapture`), management (`launchd`, `defaults`, `caffeinate`). Only `launchd-demo` currently depends on the shim library, for Mach-port-style IPC; the rest are standalone reimplementations calling POSIX/macOS APIs directly. Note that `launchd-demo` does *not* yet use `launchd/liblaunch.a` — wiring the demo onto the library is an obvious next step.

## Working on this codebase

- Version is tracked in `VERSION` and README's header — bump both together if cutting a release, per `RELEASE_NOTES.md` conventions.
- `MANIFEST.md` in `src/darwin-compat/` separates *imported* Darwin components (still none) from *clean-room* ones (the shims and the two new libraries). Don't confuse either with real ports of Apple source.
- Tests assert on observable behaviour, not return codes — a serial queue is checked to reach peak concurrency exactly 1, a throttled job to restart a bounded number of times, a reply to carry its request's correlation id. Match that bar when adding tests; a suite that only checks `== SUCCESS` would pass against a shim that silently drops messages.
- `docs/porting-priorities.md`, `docs/codebase-comparison.md`, and `docs/component-matrix.md` describe the *planned* FreeBSD/XNU integration work — useful for context on direction, but don't assume anything they describe has been built.
- Licensing is mixed per-component (BSD-2-Clause for CatBSD code, APSL for anything derived from Darwin) — check `src/patches/README.md` and individual file headers before assuming a license for new code.
