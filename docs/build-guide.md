# CatBSD Build Guide

Comprehensive guide for building CatBSD from source.

## Prerequisites

Complete the [Setup Guide](setup.md) before proceeding.

## Build Overview

CatBSD build process has three main stages:

1. **FreeBSD Base**: Build kernel and userland
2. **Darwin Components**: Build ported components
3. **Integration**: Combine and create bootable image

## Stage 1: FreeBSD Base Build

### Configure Build Environment

```bash
# Load environment
source .catbsd.env

# Navigate to FreeBSD source
cd $CATBSD_FREEBSD
```

### Build World (Userland)

```bash
# Build userland (takes 1-3 hours)
make -j$(sysctl -n hw.ncpu) buildworld

# Or with custom options
make -j$(sysctl -n hw.ncpu) buildworld \
    WITHOUT_SENDMAIL=yes \
    WITHOUT_GAMES=yes
```

### Build Kernel

```bash
# Use CatBSD kernel config
cp $CATBSD_ROOT/src/kernel/CATBSD sys/amd64/conf/

# Build kernel (takes 30-60 minutes)
make -j$(sysctl -n hw.ncpu) buildkernel KERNCONF=CATBSD
```

## Stage 2: Darwin Components

### Build launchd

```bash
cd $CATBSD_DARWIN/launchd

# Apply patches
patch -p1 < $CATBSD_ROOT/src/patches/darwin/launchd-freebsd.patch

# Build
make -f Makefile.bsd

# Install to staging
make -f Makefile.bsd install DESTDIR=$CATBSD_BUILD/stage
```

### Build libdispatch

```bash
cd $CATBSD_DARWIN/libdispatch

# Configure with CMake
mkdir build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(sysctl -n hw.ncpu)

# Install to staging
make install DESTDIR=$CATBSD_BUILD/stage
```

### Build CoreFoundation

```bash
cd $CATBSD_DARWIN/CoreFoundation

# Apply patches
patch -p1 < $CATBSD_ROOT/src/patches/darwin/cf-freebsd.patch

# Build (complex, may need custom script)
./build-cf.sh --prefix=/usr --destdir=$CATBSD_BUILD/stage
```

## Stage 3: Integration

### Install FreeBSD Base

```bash
cd $CATBSD_FREEBSD

# Install to staging directory
make installworld DESTDIR=$CATBSD_BUILD/stage
make installkernel DESTDIR=$CATBSD_BUILD/stage KERNCONF=CATBSD
make distribution DESTDIR=$CATBSD_BUILD/stage
```

### Integrate Darwin Components

```bash
# Darwin components already in staging from Stage 2
# Verify installation
ls -la $CATBSD_BUILD/stage/usr/lib/libdispatch*
ls -la $CATBSD_BUILD/stage/usr/sbin/launchd
```

### Create Filesystem Layout

```bash
cd $CATBSD_BUILD/stage

# Create macOS-style directories
mkdir -p Library/LaunchDaemons
mkdir -p Library/Frameworks
mkdir -p System/Library
mkdir -p Applications

# Copy configuration files
cp $CATBSD_ROOT/configs/* etc/
```

### Generate ISO Image

```bash
cd $CATBSD_FREEBSD/release

# Create ISO
make release \
    CHROOTDIR=$CATBSD_BUILD/chroot \
    DESTDIR=$CATBSD_BUILD/stage \
    NOPORTS=yes \
    NOSRC=yes

# ISO will be in $CATBSD_BUILD/release/
```

## Automated Build Script

For convenience, use the automated build script:

```bash
# Full build (all stages)
./scripts/build-catbsd.sh --full

# Individual stages
./scripts/build-catbsd.sh --stage freebsd
./scripts/build-catbsd.sh --stage darwin
./scripts/build-catbsd.sh --stage integrate

# Clean build
./scripts/build-catbsd.sh --clean --full
```

## Build Options

### Optimization Levels

```bash
# Debug build (slower, more info)
make buildworld CFLAGS="-O0 -g"

# Release build (faster, optimized)
make buildworld CFLAGS="-O2"

# Size-optimized build
make buildworld CFLAGS="-Os"
```

### Parallel Builds

```bash
# Use all CPU cores
make -j$(sysctl -n hw.ncpu) buildworld

# Limit to 4 cores
make -j4 buildworld
```

### Custom Features

```bash
# Build without specific components
make buildworld \
    WITHOUT_SENDMAIL=yes \
    WITHOUT_GAMES=yes \
    WITHOUT_LPR=yes

# Build with additional features
make buildworld \
    WITH_CLANG_EXTRAS=yes \
    WITH_DTRACE=yes
```

## Troubleshooting

### Build Failures

**Problem**: Compilation errors

**Solution**:

```bash
# Clean and retry
make cleanworld
make buildworld
```

**Problem**: Out of memory

**Solution**:

```bash
# Reduce parallel jobs
make -j2 buildworld

# Or increase swap space
```

### Missing Dependencies

**Problem**: Can't find headers/libraries

**Solution**:

```bash
# Ensure all dependencies installed
./scripts/setup-env.sh

# Check library paths
ldconfig -r | grep libdispatch
```

### Patch Failures

**Problem**: Patches don't apply

**Solution**:

```bash
# Check patch format
file src/patches/darwin/launchd-freebsd.patch

# Apply manually
cd src/darwin-compat/launchd
patch -p1 --dry-run < ../../patches/darwin/launchd-freebsd.patch
```

## Build Times

Approximate build times on modern hardware (4 cores, 8GB RAM):

| Stage             | Time            |
| ----------------- | --------------- |
| FreeBSD World     | 1-3 hours       |
| FreeBSD Kernel    | 30-60 min       |
| Darwin Components | 30-90 min       |
| Integration       | 15-30 min       |
| **Total**         | **2.5-5 hours** |

## Incremental Builds

### Rebuild Single Component

```bash
# Rebuild just launchd
cd src/darwin-compat/launchd
make -f Makefile.bsd clean
make -f Makefile.bsd
make -f Makefile.bsd install DESTDIR=$CATBSD_BUILD/stage
```

### Rebuild Kernel Only

```bash
cd $CATBSD_FREEBSD
make buildkernel KERNCONF=CATBSD
make installkernel KERNCONF=CATBSD DESTDIR=$CATBSD_BUILD/stage
```

## Testing the Build

### Boot in VM

```bash
# Using QEMU
qemu-system-x86_64 \
    -cdrom $CATBSD_BUILD/release/CatBSD-*.iso \
    -m 2048 \
    -smp 2 \
    -boot d

# Using VirtualBox
VBoxManage createvm --name CatBSD --register
VBoxManage modifyvm CatBSD --memory 2048 --cpus 2
VBoxManage storagectl CatBSD --name IDE --add ide
VBoxManage storageattach CatBSD --storagectl IDE \
    --port 0 --device 0 --type dvddrive \
    --medium $CATBSD_BUILD/release/CatBSD-*.iso
VBoxManage startvm CatBSD
```

### Verify Components

```bash
# After booting CatBSD:

# Check kernel
uname -a
# Should show: CatBSD

# Check launchd
ps aux | grep launchd

# Check libraries
ldconfig -r | grep -E "dispatch|CoreFoundation"

# Test Darwin components
launchctl list
```

## Next Steps

After successful build:

1. Test in VM
2. Document any issues
3. Run test suite
4. Create bug reports if needed

## Resources

- [FreeBSD Build Documentation](https://docs.freebsd.org/en/books/handbook/cutting-edge/)
- [FreeBSD Release Engineering](https://docs.freebsd.org/en/articles/releng/)
- [CMake Documentation](https://cmake.org/documentation/)
