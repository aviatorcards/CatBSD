# CatBSD Troubleshooting

Common issues and solutions when building and running CatBSD.

## Build Issues

### Compilation Errors

#### Missing Headers

**Symptom**:

```
fatal error: 'mach/mach.h' file not found
```

**Solution**:

```bash
# Ensure shims are in include path
export CFLAGS="-I$CATBSD_ROOT/src/darwin-compat/shims"

# Or add to Makefile
CFLAGS+= -I../shims
```

#### Undefined Symbols

**Symptom**:

```
ld: undefined reference to 'mach_absolute_time'
```

**Solution**:

```bash
# Link compatibility library
LDADD+= -L../shims -ldarwin_compat

# Or implement missing function in shim
```

### Build System Issues

#### Make Version Mismatch

**Symptom**:

```
make: invalid option -- j
```

**Solution**:

```bash
# Use GNU make on FreeBSD
gmake -j4 buildworld

# Or install GNU make
pkg install gmake
```

#### Out of Disk Space

**Symptom**:

```
No space left on device
```

**Solution**:

```bash
# Clean build artifacts
make cleanworld
rm -rf $CATBSD_BUILD/*

# Check disk usage
df -h
du -sh $CATBSD_ROOT/*
```

## Runtime Issues

### Boot Problems

#### Kernel Panic on Boot

**Symptom**: System crashes during boot

**Solution**:

```bash
# Boot with verbose mode
# At boot loader, press 3 for verbose

# Check kernel config
diff src/kernel/CATBSD src/freebsd/sys/amd64/conf/GENERIC

# Try GENERIC kernel first
make buildkernel KERNCONF=GENERIC
```

#### Stuck at "Mounting root"

**Symptom**: Boot hangs waiting for root filesystem

**Solution**:

```bash
# Check filesystem in ISO
# Rebuild with correct root device

# In kernel config:
options     ROOTDEVNAME=\"ufs:/dev/ada0\"
```

### Service Issues

#### launchd Won't Start

**Symptom**:

```
launchd: mach_port_allocate failed
```

**Solution**:

```bash
# Check shim implementation
cat src/darwin-compat/shims/mach_port.c

# Enable debug logging
launchd -d

# Check permissions
ls -la /usr/sbin/launchd
# Should be: -rwxr-xr-x root wheel
```

#### Library Loading Failures

**Symptom**:

```
Shared object "libdispatch.so.1" not found
```

**Solution**:

```bash
# Update library cache
ldconfig

# Check library path
ldconfig -r | grep dispatch

# Add to /etc/rc.conf
ldconfig_paths="/usr/local/lib /usr/lib"
```

## VM Issues

### QEMU Problems

#### No Display Output

**Symptom**: Black screen in QEMU

**Solution**:

```bash
# Add graphics options
qemu-system-x86_64 \
    -cdrom CatBSD.iso \
    -vga std \
    -display gtk

# Or use serial console
qemu-system-x86_64 \
    -cdrom CatBSD.iso \
    -nographic \
    -serial mon:stdio
```

#### Slow Performance

**Symptom**: VM is very slow

**Solution**:

```bash
# Enable KVM acceleration (Linux host)
qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -m 4096 \
    -smp 4

# Use virtio drivers
-device virtio-net-pci \
-device virtio-blk-pci
```

### VirtualBox Problems

#### Guest Additions Won't Install

**Symptom**: Can't install VirtualBox Guest Additions

**Solution**:

```bash
# Install FreeBSD packages first
pkg install virtualbox-ose-additions

# Or build from source
cd /usr/ports/emulators/virtualbox-ose-additions
make install clean
```

## Development Issues

### Git Problems

#### Submodule Issues

**Symptom**:

```
fatal: not a git repository: src/freebsd/.git
```

**Solution**:

```bash
# Initialize submodules
git submodule init
git submodule update

# Or clone with submodules
git clone --recursive https://github.com/yourusername/CatBSD.git
```

### Debugging

#### Core Dumps

**Enable core dumps**:

```bash
# In /etc/sysctl.conf
kern.coredump=1
kern.corefile=/tmp/%N.core

# Set ulimit
ulimit -c unlimited
```

**Analyze core dump**:

```bash
lldb /usr/sbin/launchd /tmp/launchd.core
(lldb) bt
(lldb) frame select 0
(lldb) print variable_name
```

#### Debug Symbols

**Build with debug symbols**:

```bash
# Add to make.conf
CFLAGS+= -g -O0
STRIP=

# Or per-component
make DEBUG_FLAGS="-g -O0"
```

## Performance Issues

### Slow Boot

**Symptom**: Takes too long to boot

**Solution**:

```bash
# Profile boot time
sysctl kern.boottime

# Check service startup times
launchctl list | grep -v "^-"

# Disable unnecessary services
launchctl unload /Library/LaunchDaemons/unnecessary.plist
```

### High Memory Usage

**Symptom**: System using too much RAM

**Solution**:

```bash
# Check memory usage
top -o res

# Find memory leaks
vmstat -s
ps aux --sort=-rss | head

# Use DTrace to profile
dtrace -n 'pid$target:::entry { @[ustack()] = count(); }' -p PID
```

## Common Error Messages

### "Operation not permitted"

**Cause**: Permission issue or security policy

**Solution**:

```bash
# Check file permissions
ls -la /path/to/file

# Run with sudo if needed
sudo command

# Check Capsicum/MAC policies
sysctl security.mac
```

### "Protocol not supported"

**Cause**: Missing network protocol or driver

**Solution**:

```bash
# Check kernel config
grep -i network src/kernel/CATBSD

# Load kernel module
kldload if_em  # Example for Intel NIC

# Check available protocols
netstat -s
```

### "Cannot allocate memory"

**Cause**: Out of RAM or kernel limits

**Solution**:

```bash
# Increase swap
swapon -a

# Adjust kernel limits in /boot/loader.conf
kern.maxusers=128
kern.ipc.shmmni=256

# Reboot to apply
```

## Getting Help

### Collect Debug Information

```bash
# System information
uname -a
freebsd-version

# Hardware info
dmesg | head -50

# Loaded modules
kldstat

# Running processes
ps aux

# Network status
ifconfig
netstat -rn

# Disk usage
df -h
gpart show
```

### Create Bug Report

Include:

1. CatBSD version/commit
2. Hardware specs (CPU, RAM)
3. VM or bare metal
4. Error messages (full output)
5. Steps to reproduce
6. Debug information from above

### Resources

- GitHub Issues: Report bugs
- FreeBSD Forums: General BSD help
- ravynOS Community: Similar project insights
- Stack Overflow: Programming questions

## Known Issues

### Current Limitations

1. **GUI Not Implemented**: CLI only for now
2. **Limited Darwin Components**: Only core libraries ported
3. **No Application Support**: Can't run macOS apps yet
4. **Experimental Status**: Expect bugs and crashes

### Workarounds

- Use FreeBSD equivalents where Darwin components fail
- Test in VM before bare metal
- Keep backups of working builds
- Document issues for future reference

## Prevention Tips

1. **Test Incrementally**: Build and test each component
2. **Use Version Control**: Commit working states
3. **Keep Logs**: Save build and boot logs
4. **Document Changes**: Note what you modified
5. **Backup Configs**: Save working configurations
