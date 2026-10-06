# FreeBSD VM Setup Guide

How to get a FreeBSD VM running on your Mac so you can build and test CatBSD
on the real target platform.

---

## Local Baseline ISO (Present in Tree)

The project currently has the verified official release ISO in `src/freebsd/`:

* **File**: `src/freebsd/FreeBSD-14.5-RELEASE-amd64-disc1.iso` (1.32 GB)
* **Architecture**: `amd64` (x86_64 — Tier 1 Target)
* **Format**: ISO 9660 bootable disc image
* **Contents**: Includes full base distribution (`base.txz`, `kernel.txz`, and full source tree `src.txz`)

You do not need to download an ISO if you use this file! Point your VM directly to:
```bash
/Users/tristan/Documents/code/CatBSD/src/freebsd/FreeBSD-14.5-RELEASE-amd64-disc1.iso
```

---

## Alternative Downloads (Snapshots & Other Architectures)

If you need a different architecture or want weekly snapshots:

| Architecture | Image | Size | Use when |
|---|---|---|---|
| **amd64** | `FreeBSD-14.5-RELEASE-amd64-disc1.iso` | 1.3 GB | Primary target (in `src/freebsd/`) |
| **arm64** | [FreeBSD Snapshots Index](https://download.freebsd.org/snapshots/) | ~1 GB | Apple Silicon native testing |

### Fast Alternative: Pre-built VM Disk Images
If you want to skip the installer entirely, FreeBSD publishes ready-to-run raw and QCOW2 images that boot straight to a login prompt:
* [FreeBSD Official VM Images](https://download.freebsd.org/releases/VM-IMAGES/)
* Decompress `.qcow2.xz` or `.raw.xz` and import directly into UTM as a drive.

```bash
# Decompress after download
xz -d FreeBSD-*.iso.xz
```

> [!TIP]
> Always match the ISO architecture to your Mac's CPU. Using amd64 on Apple
> Silicon requires Rosetta-based emulation and is much slower than native arm64.

---

## Option A — UTM (Apple Silicon Mac)

[UTM](https://mac.getutm.app) supports running both native ARM VMs and emulating `x86_64` (`amd64`).

### Install UTM

```bash
brew install --cask utm
# or download directly from https://mac.getutm.app
```

### Running the amd64 ISO in UTM:

Since the ISO is **amd64** (`FreeBSD-14.5-RELEASE-amd64-disc1.iso`), select **Emulate** in UTM:

1. Open UTM → **+** → **Emulate**
2. Operating System: **Other**
3. Browse to the ISO: `src/freebsd/FreeBSD-14.5-RELEASE-amd64-disc1.iso`
4. Architecture: **x86_64**
5. RAM: **4096 MB**, CPU Cores: **4**
6. Storage: **20 GB**
7. Name: `FreeBSD 14.5 amd64`
8. **Save** → **Play** ▶

*(Note: UTM uses QEMU x86_64 emulation under the hood. It compiles slightly slower than native ARM, but runs true 64-bit x86 instructions identical to PC hardware).*

### Install FreeBSD

The FreeBSD installer is menu-driven. Quick path:

```
1. Boot Multi user [Enter]
2. Install → [Enter]
3. Keymap: default → Select
4. Hostname: catbsd-dev
5. Components: (uncheck doc/lib32/ports — optional, saves space)
6. Network: vtnet0 → Yes (DHCP) → Yes (IPv4)
7. Mirror: pick the closest one
8. Guided UFS → Entire Disk → GPT → Finish → Commit
9. Root password: set one
10. SSHD: Yes
11. NTP: Yes
12. Exit installer → Reboot
```

After reboot, eject the ISO (UTM: Drive menu → Eject) and log in as `root`.

### Share the CatBSD source tree (virtio-9p)

In UTM, add a shared folder:
- **VM Settings** → **Sharing** → enable **Share Directory**
- Point it at your CatBSD repo directory

Inside the VM:

```bash
# FreeBSD 14+ — virtio-9p is in base
mount -t virtfs -o trans=virtio,version=9p2000.L myshare /mnt
# Or use the UTM-generated automount path (check /mnt after boot)
```

Alternatively, just use `scp` or `git clone` inside the VM.

---

## Option B — VirtualBox (Intel Mac)

> [!NOTE]
> VirtualBox support for Apple Silicon is in beta as of 2026. Use UTM on M-series Macs.

### Install VirtualBox

```bash
brew install --cask virtualbox
```

### Create the VM

```
1. New → Name: FreeBSD 14.2, Type: BSD, Version: FreeBSD (64-bit)
2. RAM: 2048 MB
3. Hard disk: Create VDI, 20 GB, dynamically allocated
4. Settings → Storage → IDE Controller → add .iso
5. Settings → Network → Adapter 1: NAT
6. Start
```

Install FreeBSD the same way as above.

### Share source via NFS (VirtualBox)

On your Mac (host):

```bash
# Add to /etc/exports
/Users/yourname/Documents/code/CatBSD -network 192.168.56.0 -mask 255.255.255.0

# Start NFS
sudo nfsd start
```

Inside the VM:

```bash
mount -t nfs 10.0.2.2:/Users/yourname/Documents/code/CatBSD /mnt/catbsd
```

---

## First Boot Setup

After installing and logging in:

```bash
# Update pkg
pkg update

# Install clang if not already default
pkg install llvm    # for -fblocks support on older installs

# Verify clang supports Blocks
echo 'int main(){void(^b)(void)=^{};b();return 0;}' | \
    cc -fblocks -xc - -o /dev/null && echo "Blocks: OK"

# Clone or mount the CatBSD source
# If using git:
pkg install git
git clone https://github.com/YOUR_USER/CatBSD.git /root/CatBSD
```

---

## Running the Test Suite

```bash
cd /root/CatBSD   # or wherever you mounted it

# Full suite (build + all tests)
sh scripts/test-freebsd.sh

# Libraries only (faster)
sh scripts/test-freebsd.sh libs

# Build only (check for compile errors)
sh scripts/test-freebsd.sh build
```

### Expected output on a clean FreeBSD 14 install

```
=== CatBSD FreeBSD Test Suite ===
Host:    FreeBSD 14.2-RELEASE
Arch:    aarch64
...

=== Building libraries ===
  Building blocks (libBlocksRuntime)...  ok
  Building shims (libdarwin_compat)...   ok
  Building libdispatch subset...         ok
  Building launchd (liblaunch)...        ok
  Building catbsd-init...                ok
  Building catbsd-launchctl...           ok

=== Library test suites ===
  ✓ Blocks Runtime — stack, copy/release, capture, nested
  ✓ Mach port shim + XPC shim
  ✓ libdispatch — serial, concurrent, barriers, timers
  ✓ liblaunch — plist, supervision, KeepAlive, sockets
  ✓ launchctl protocol — all 6 verbs over XPC

=== catbsd-init smoke tests ===
  ✓ catbsd-init — empty-dir, single-user, daemon-load

All 6 test suites passed on FreeBSD 14.2-RELEASE!
```

---

## Trying catbsd-init Interactively

Once the tests pass, you can run the full stack:

```bash
cd /root/CatBSD/src/catbsd-init

# Terminal 1 — start the supervisor
CATBSD_INIT_DAEMON_DIR=../../etc/catbsd/daemons \
CATBSD_INIT_LOG_LEVEL=2 \
./catbsd-init

# Terminal 2 (ssh into the VM) — use launchctl
cd /root/CatBSD/src/darwin-compat/launchctl-demo
./catbsd-launchctl list
./catbsd-launchctl status com.catbsd.syslog
./catbsd-launchctl stop  com.catbsd.syslog
./catbsd-launchctl start com.catbsd.syslog
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `cc: error: unrecognized argument: -fblocks` | Install LLVM: `pkg install llvm` and use `clang` explicitly |
| `kqueue: No such file or directory` | Shouldn't happen on FreeBSD — check you're not in a jail without kqueue |
| `XPC socket permission denied` | Check `$TMPDIR` is writable; try `CATBSD_XPC_RUNTIME_DIR=/tmp ./catbsd-launchctl list` |
| Test times out in `test-launchctl` | May need `ulimit -n 256` — XPC uses AF_UNIX sockets |
| `make: pthread: not found` | Shouldn't happen — pthreads is in base. Check `cc` is clang not gcc |

---

## Reporting Results

Once you have results (pass or fail), please update
[`docs/freebsd-status.md`](freebsd-status.md) with:

- FreeBSD version and architecture
- Which tests passed / failed
- Any compiler warnings or errors encountered

This is the most valuable contribution at this stage of the project.
