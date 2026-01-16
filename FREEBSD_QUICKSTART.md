# CatBSD FreeBSD Quick Start

Your FreeBSD VM is running! Here's how to get CatBSD building.

## Step 1: Setup FreeBSD Environment

**Inside the VM** (via console or SSH):

```bash
# Copy setup script to VM first (from macOS)
scp -P 2222 freebsd-setup.sh user@localhost:~/

# Then inside VM, run:
sh ~/freebsd-setup.sh
```

This installs: git, clang, gmake, libxml2, rsync

---

## Step 2: Sync CatBSD Source

**From macOS**:

```bash
./sync-to-vm.sh your-username
```

This copies the CatBSD source to `~/CatBSD` in the VM.

---

## Step 3: Build Shim Library

**Inside VM**:

```bash
cd ~/CatBSD/src/darwin-compat/shims
gmake clean
gmake
```

Expected issues to fix:

- Platform-specific headers
- pthread differences
- kqueue implementation

---

## Step 4: Build First Utility

**Inside VM**:

```bash
cd ~/CatBSD/src/darwin-compat/sw_vers-demo
gmake clean
gmake
./sw_vers-demo
```

---

## SSH Access

```bash
# From macOS
ssh -p 2222 user@localhost

# File transfer
scp -P 2222 file.txt user@localhost:~/
scp -P 2222 user@localhost:~/file.txt .
```

---

## Troubleshooting

### Build errors

- Check compiler: `clang --version`
- Check gmake: `gmake --version`
- Missing headers: `pkg search <header-name>`

### Network issues

- Test: `ping 8.8.8.8`
- Check sshd: `sudo service sshd status`

---

## Next Steps

1. Fix shim library for FreeBSD
2. Port utilities one by one
3. Document FreeBSD-specific changes
4. Create FreeBSD package

See Phase 4 implementation plan for details.
