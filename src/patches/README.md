# Patch Management

This directory contains patches for integrating Darwin components with FreeBSD.

## Directory Structure

```
patches/
├── README.md           # This file
├── freebsd/            # Patches to FreeBSD source
│   └── (future patches)
└── darwin/             # Patches to Darwin components
    ├── launchd-freebsd.patch
    ├── libdispatch-freebsd.patch
    └── cf-freebsd.patch
```

## Patch Naming Convention

Format: `<component>-<description>.patch`

Examples:

- `launchd-freebsd.patch` - FreeBSD compatibility for launchd
- `libdispatch-kqueue.patch` - Use kqueue instead of workqueues
- `cf-build-system.patch` - Build system adaptations

## Creating Patches

### Method 1: Using diff

```bash
# Make a copy of original
cd src/darwin-compat
cp -r launchd launchd.orig

# Make your changes to launchd/
# ... edit files ...

# Generate patch
diff -Naur launchd.orig/ launchd/ > ../../patches/darwin/launchd-freebsd.patch

# Clean up
rm -rf launchd.orig
```

### Method 2: Using git

```bash
cd src/darwin-compat/launchd

# Make changes and commit
git add .
git commit -m "FreeBSD compatibility changes"

# Generate patch
git format-patch -1 HEAD -o ../../../patches/darwin/

# Rename if needed
mv ../../../patches/darwin/0001-*.patch ../../../patches/darwin/launchd-freebsd.patch
```

## Applying Patches

### Apply Single Patch

```bash
cd src/darwin-compat/launchd
patch -p1 < ../../../patches/darwin/launchd-freebsd.patch
```

### Dry Run (Test Without Applying)

```bash
patch -p1 --dry-run < ../../../patches/darwin/launchd-freebsd.patch
```

### Reverse Patch

```bash
patch -p1 -R < ../../../patches/darwin/launchd-freebsd.patch
```

## Patch Format

Patches should be in unified diff format (`diff -u` or `diff -Naur`).

Example patch structure:

```diff
--- a/src/launchd.c
+++ b/src/launchd.c
@@ -10,7 +10,12 @@
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

## Patch Guidelines

### 1. Keep Patches Focused

- One logical change per patch
- Separate build system changes from code changes
- Split large patches into smaller, reviewable pieces

### 2. Document Changes

Add comments explaining non-obvious changes:

```c
#ifdef FREEBSD_COMPAT
/* FreeBSD doesn't have Mach ports, use kqueue instead */
int kq = kqueue();
#else
mach_port_t port = mach_port_allocate();
#endif
```

### 3. Preserve Original Code

Use `#ifdef FREEBSD_COMPAT` to keep original Darwin code:

```c
#ifdef FREEBSD_COMPAT
    // FreeBSD version
#else
    // Original Darwin version
#endif
```

### 4. Include Context

Use `diff -u` with at least 3 lines of context (default).

## Patch Workflow

### Initial Port

1. Extract component with `extract-darwin-component.sh`
2. Make FreeBSD compatibility changes
3. Generate patch
4. Test patch application on clean source
5. Document patch in component's `PORTING_CHECKLIST.md`

### Updating Patches

1. Apply existing patch
2. Make additional changes
3. Regenerate patch
4. Test on clean source
5. Update patch documentation

### Patch Maintenance

- Keep patches up to date with upstream Darwin changes
- Regularly test patch application
- Document patch dependencies
- Version patches if needed

## Patch Testing

### Test Script Template

```bash
#!/bin/sh
# Test patch application

COMPONENT="launchd"
PATCH="patches/darwin/${COMPONENT}-freebsd.patch"

# Clean start
cd src/darwin-compat
rm -rf ${COMPONENT}

# Extract fresh copy
../../scripts/extract-darwin-component.sh ${COMPONENT}

# Apply patch
cd ${COMPONENT}
patch -p1 < ../../../${PATCH}

if [ $? -eq 0 ]; then
    echo "✓ Patch applied successfully"

    # Try to build
    make -f Makefile.bsd

    if [ $? -eq 0 ]; then
        echo "✓ Build successful"
    else
        echo "✗ Build failed"
        exit 1
    fi
else
    echo "✗ Patch failed to apply"
    exit 1
fi
```

## Patch Repository

All patches are version controlled in the CatBSD repository:

- Commit patches with descriptive messages
- Tag major patch versions
- Document breaking changes

## Troubleshooting

### Patch Fails to Apply

**Problem**: `patch: **** malformed patch at line X`

**Solution**:

- Check patch format (should be unified diff)
- Ensure correct line endings (Unix LF, not Windows CRLF)
- Regenerate patch

**Problem**: `Hunk #X FAILED`

**Solution**:

- Source code has changed
- Apply manually and regenerate patch
- Check if patch is outdated

### Conflicts

If patches conflict:

1. Apply patches in order
2. Resolve conflicts manually
3. Consider combining patches
4. Document patch dependencies

## Best Practices

1. **Test Before Committing**: Always test patches on clean source
2. **Keep Patches Small**: Easier to review and maintain
3. **Document Well**: Explain why changes were made
4. **Version Control**: Commit patches to repository
5. **Regular Updates**: Keep patches current with upstream

## References

- [GNU diff Manual](https://www.gnu.org/software/diffutils/manual/diffutils.html)
- [patch(1) Manual](https://www.freebsd.org/cgi/man.cgi?query=patch)
- [FreeBSD Porter's Handbook - Patching](https://docs.freebsd.org/en/books/porters-handbook/slow-porting/)

---

For questions or issues, see the main CatBSD documentation.
