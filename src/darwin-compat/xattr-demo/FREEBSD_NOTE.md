# xattr-demo - FreeBSD Compatibility Note

## Issue

FreeBSD uses a different extended attributes API than macOS:

- macOS: `sys/xattr.h` with `getxattr()`, `setxattr()`, etc.
- FreeBSD: `sys/extattr.h` with `extattr_get_file()`, `extattr_set_file()`, etc.

## Temporary Solution

For now, skip `xattr-demo` on FreeBSD. It requires significant API translation.

## Future Fix

Create a compatibility wrapper that translates between the two APIs:

- macOS `getxattr()` → FreeBSD `extattr_get_file(EXTATTR_NAMESPACE_USER, ...)`
- macOS `setxattr()` → FreeBSD `extattr_set_file(EXTATTR_NAMESPACE_USER, ...)`

## Alternative

Use a library like `libattr` that provides cross-platform xattr support.

---

For now, focus on utilities that don't use xattr:

- ✅ sw_vers (working!)
- ⏭️ Try scutil, networksetup, open, etc.
