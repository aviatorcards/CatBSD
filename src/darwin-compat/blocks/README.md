# CatBSD Blocks Runtime

The [LLVM `BlocksRuntime`](https://github.com/llvm/llvm-project/tree/main/compiler-rt/lib/BlocksRuntime)
imported verbatim from `compiler-rt`, with a hand-written `config.h`
replacing autoconf output.

This is the lowest layer in the Darwin compatibility stack. Everything above
it — libdispatch, CoreFoundation, launchd — depends on Blocks syntax.

## What it provides

```c
#include <Block.h>

void *_Block_copy(const void *block);     /* promote stack block to heap */
void  _Block_release(const void *block);  /* release heap block          */

/* Type-correct wrappers (use these, not the underscored functions): */
#define Block_copy(b)    ...
#define Block_release(b) ...
```

Blocks syntax itself (`^{ ... }`, `__block`, captured variables) is handled
by the compiler (`clang -fblocks` or `gcc -fblocks`). This library supplies
the runtime support those compiler-generated calls reach into.

## Files

| File | Origin | Notes |
|------|--------|-------|
| `runtime.c` | LLVM compiler-rt | Upstream, unmodified |
| `Block.h` | LLVM compiler-rt | Upstream, unmodified |
| `Block_private.h` | LLVM compiler-rt | Upstream, unmodified |
| `config.h` | CatBSD | Replaces autoconf — uses `__sync` builtins |
| `Makefile` | CatBSD | Builds `libBlocksRuntime.a` |
| `tests/test_blocks.c` | CatBSD | Smoke tests (stack, copy, `__block`, nested) |

## Building

```sh
make          # produces libBlocksRuntime.a
make test     # build + run tests/test_blocks
make clean
```

Requires `clang` or `gcc` with `-fblocks` support (standard on macOS and
FreeBSD; on Linux, install `clang`).

## Linking

```makefile
CFLAGS  += -fblocks -I/path/to/blocks
LDFLAGS += -L/path/to/blocks -lBlocksRuntime
```

On **macOS** the system `libSystem` already provides a Blocks runtime, so
`-lBlocksRuntime` is only needed on FreeBSD and Linux. The Makefile in each
component that uses Blocks handles this with a platform guard.

## Status

| Property | State |
|----------|-------|
| Builds | macOS (clang), Linux (clang/gcc) |
| Tested | macOS — all tests pass, zero warnings |
| FreeBSD | Expected to work (POSIX + clang -fblocks); not yet run |
| Dependencies | none beyond libc |

## License

MIT — see the copyright header in `runtime.c`. Compatible with CatBSD's
BSD-2-Clause and the APSL components it will support.

## What this unblocks

```
blocks/libBlocksRuntime.a        ← this component
    ↓
swift-corelibs-libdispatch       ← next import
    ↓
swift-corelibs-foundation        ← CoreFoundation
    ↓
Libsystem shims → launchd
```

The clean-room `libdispatch/` subset already works without Blocks, but the
full upstream `swift-corelibs-libdispatch` requires Blocks syntax in its
public headers. This import removes that blocker.
