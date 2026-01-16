# FreeBSD VM for CatBSD Development

QEMU-based FreeBSD virtual machine for testing CatBSD utilities.

## Quick Start

### 1. Install FreeBSD

```bash
./install-vm.sh
```

Follow the FreeBSD installer:

- Hostname: `catbsd-dev`
- Partitioning: Auto (ZFS)
- Enable: sshd, ntpd, dumpdev
- Create user and add to `wheel` group

### 2. Run VM

```bash
./run-vm.sh
```

### 3. SSH Access

```bash
ssh -p 2222 user@localhost
```

### 4. Sync CatBSD Source

```bash
./sync-to-vm.sh your-username
```

---

## VM Configuration

- **OS**: FreeBSD 14.3-RELEASE
- **Disk**: 20GB (qcow2)
- **Memory**: 4GB
- **CPUs**: 4 cores
- **Network**: User mode with SSH forwarding (port 2222)
- **Acceleration**: HVF (macOS hypervisor)

---

## Initial Setup (Inside VM)

```bash
# Update system
sudo freebsd-update fetch install

# Install packages
sudo pkg install git clang gmake libxml2 rsync

# Build CatBSD shims
cd ~/CatBSD/src/darwin-compat/shims
gmake

# Test
gmake test
```

---

## Workflow

### Daily Development

1. **Start VM**: `./run-vm.sh`
2. **SSH in**: `ssh -p 2222 user@localhost`
3. **Sync changes**: `./sync-to-vm.sh user`
4. **Build & test**: Inside VM

### File Transfer

```bash
# Copy to VM
scp -P 2222 file.txt user@localhost:~/

# Copy from VM
scp -P 2222 user@localhost:~/file.txt .
```

---

## Scripts

- **`install-vm.sh`** - One-time FreeBSD installation
- **`run-vm.sh`** - Daily VM launcher
- **`sync-to-vm.sh`** - Sync CatBSD source to VM

---

## Troubleshooting

### VM won't start

- Check if another QEMU instance is running
- Verify disk image exists: `ls vm/catbsd-freebsd.qcow2`

### Can't SSH

- Wait 30 seconds after boot
- Check sshd is enabled: `sudo service sshd status`
- Verify port forwarding: `nc -z localhost 2222`

### Slow performance

- Increase memory in scripts (edit `MEMORY="4G"`)
- Increase CPUs (edit `CPUS="4"`)
- Check HVF acceleration is working

---

## VM Management

### Shutdown

```bash
# Graceful shutdown (inside VM)
sudo shutdown -p now

# Or from QEMU monitor
quit
```

### Snapshots

```bash
# Create snapshot
qemu-img snapshot -c before-changes vm/catbsd-freebsd.qcow2

# List snapshots
qemu-img snapshot -l vm/catbsd-freebsd.qcow2

# Restore snapshot
qemu-img snapshot -a before-changes vm/catbsd-freebsd.qcow2
```

### Disk Info

```bash
qemu-img info vm/catbsd-freebsd.qcow2
```

---

## Next Steps

After VM is set up:

1. Build shim library on FreeBSD
2. Port utilities one by one
3. Run tests on FreeBSD
4. Document FreeBSD-specific changes

See [Phase 4 Implementation Plan](../.gemini/antigravity/brain/*/implementation_plan.md)
