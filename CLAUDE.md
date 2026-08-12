# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo actually is right now

CatBSD's long-term goal (see `PLAN.md` and `docs/architecture.md`) is a hybrid OS: FreeBSD kernel/userland + ported Darwin components (launchd, libdispatch, CoreFoundation, etc.), eventually bootable. **That goal is not implemented yet.** `src/freebsd/` and `src/apple-oss/` are empty — the source-fetch/import pipeline described in the docs has not been run in this checkout.

What actually exists and works today is `src/darwin-compat/`: 15 small, self-contained C demo programs ("The Essential 15") that reimplement Darwin/macOS CLI tool *interfaces* (not ported Apple source) on top of a tiny compatibility shim library, and build/run natively on macOS. Treat the rest of the tree (`src/kernel/`, `src/patches/`, `scripts/*.sh` for fetching/importing FreeBSD+Apple sources) as scaffolding for future phases, not working infrastructure — several of these scripts assume tools like `darwinbuild` and multi-GB source trees that aren't present here.

When asked to "build CatBSD" or "run the tests," it almost always means working within `src/darwin-compat/`.

## Common commands

All from `src/darwin-compat/`:

```bash
# Build everything (shim library + all 5 "core" demos referenced in build-all.sh)
./build-all.sh

# Build a single utility
cd <name>-demo && make            # produces ./<name>-demo binary
cd <name>-demo && make test       # most demos have a `make test` target
cd <name>-demo && make clean

# Build/test the shim library alone
cd shims && make
cd shims/tests && ./run_tests.sh  # rebuilds shims + compiles/runs test_shims

# Run the showcase demos
./demo.sh              # original 5-utility demo
./demo-essential-15.sh # full 15-utility demo
```

Note: `build-all.sh` only builds/cleans 5 of the 15 utilities (`sw_vers-demo plutil-demo say-demo caffeinate-demo launchd-demo`). The other 10 (`afplay`, `ditto`, `defaults`, `networksetup`, `open`, `pbcopy`, `pbpaste`, `screencapture`, `scutil`, `xattr`) must be built individually with `make` in their own directory. If asked to build "everything," build all 15 directories, not just what `build-all.sh` covers.

There is no top-level build system, CI config, or linter in this repo — each utility is an independent Makefile-based C project.

## Architecture of `src/darwin-compat/`

```
darwin-compat/
├── shims/                  # libdarwin_compat.a — the only shared library
│   ├── mach_port.{c,h}     # Mach port IPC emulated via kqueue/pipes
│   ├── darwin_syscalls.{c,h}
│   └── tests/
├── <name>-demo/            # one directory per utility, each independent
│   ├── <name>-demo.c
│   ├── Makefile
│   └── (test fixtures, e.g. demo.plist)
├── build-all.sh
├── demo.sh / demo-essential-15.sh
└── MANIFEST.md             # tracks planned (not yet imported) Darwin components
```

Each `<name>-demo/Makefile` follows the same BSD-style pattern (`CC`, `CFLAGS`, `PROG`, `SRCS`, `.c.o` suffix rule, `all`/`clean`/`test` targets) — copy this pattern when adding a new utility rather than inventing a new build style. The shim library's Makefile additionally defines `-DFREEBSD_COMPAT` and builds a static archive (`ar`/`ranlib`), since it's meant to be portable to FreeBSD even though it currently only gets exercised on macOS.

The 15 utilities are grouped by theme (see README.md): system/config (`sw_vers`, `scutil`, `networksetup`), file ops (`plutil`, `xattr`, `ditto`, `open`), clipboard (`pbcopy`, `pbpaste`), media (`say`, `afplay`, `screencapture`), management (`launchd`, `defaults`, `caffeinate`). Only `launchd-demo` currently depends on the shim library for Mach-port-style IPC; the rest are largely standalone reimplementations calling POSIX/macOS APIs directly.

## Working on this codebase

- Version is tracked in `VERSION` and README's header — bump both together if cutting a release, per `RELEASE_NOTES.md` conventions.
- `MANIFEST.md` in `src/darwin-compat/` is the tracking doc for actually-ported (vs. reimplemented) Darwin components; it currently lists none as imported. Don't confuse the demo utilities here with real ports of Apple source.
- `docs/porting-priorities.md`, `docs/codebase-comparison.md`, and `docs/component-matrix.md` describe the *planned* FreeBSD/XNU integration work — useful for context on direction, but don't assume anything they describe has been built.
- Licensing is mixed per-component (BSD-2-Clause for CatBSD code, APSL for anything derived from Darwin) — check `src/patches/README.md` and individual file headers before assuming a license for new code.
