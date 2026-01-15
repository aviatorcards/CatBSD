# CatBSD Setup Guide

Complete guide for setting up your CatBSD development environment.

## Prerequisites

### Hardware Requirements

- **CPU**: x86_64 processor (ARM64 support planned)
- **RAM**: 8GB minimum, 16GB recommended
- **Disk**: 40GB free space minimum
- **VM Software** (recommended): VirtualBox, VMware, or bhyve

### Recommended Host OS

**FreeBSD 13.x or later** is the recommended build host as it's closest to the target system.

Alternative hosts:

- Linux (Ubuntu 22.04+, Fedora 38+)
- macOS (for cross-compilation experiments)

## Step 1: Install FreeBSD (VM Recommended)

### Using VirtualBox

1. Download FreeBSD 13.x ISO from [freebsd.org](https://www.freebsd.org/where/)
2. Create new VM:
   - Type: BSD
   - Version: FreeBSD (64-bit)
   - RAM: 4GB minimum
   - Disk: 40GB VDI
3. Install FreeBSD with default options
4. Enable SSH for remote access (optional)

### Using bhyve (FreeBSD Host)

```bash
# Install vm-bhyve
pkg install vm-bhyve

# Initialize
sysrc vm_enable="YES"
vm init
vm switch create public
vm switch add public em0

# Create and install VM
vm create -t freebsd catbsd-dev
vm install catbsd-dev FreeBSD-13.2-RELEASE-amd64.iso
vm start catbsd-dev
```

## Step 2: Install Dependencies

### On FreeBSD

```bash
# Update package repository
pkg update

# Install build tools
pkg install -y git subversion clang llvm cmake ninja python3 gmake

# Optional: Install additional tools
pkg install -y vim tmux htop
```

### On Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y git subversion clang llvm cmake ninja-build \
    python3 build-essential libssl-dev
```

### On macOS

```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install git subversion llvm cmake ninja python3
```

## Step 3: Clone CatBSD Repository

```bash
# Clone the repository
git clone https://github.com/yourusername/CatBSD.git
cd CatBSD

# Run setup script
./scripts/setup-env.sh
```

The setup script will:

- Verify all dependencies are installed
- Create build directories
- Generate `.catbsd.env` configuration file

## Step 4: Configure Environment

```bash
# Load environment variables
source .catbsd.env

# Verify configuration
echo $CATBSD_ROOT
echo $MACOS_VERSION
```

You can customize settings in `.catbsd.env`:

- `MACOS_VERSION`: Target macOS version (default: 10.8.5)
- `FREEBSD_BRANCH`: FreeBSD branch to use (default: stable/13)

## Step 5: Fetch Source Trees

```bash
# Download FreeBSD and Apple OSS sources
./scripts/fetch-sources.sh
```

This will:

- Clone FreeBSD source tree (~2GB)
- Clone Apple OSS distribution (~1GB)
- Create source inventory document

**Note**: First run may take 15-30 minutes depending on connection speed.

## Step 6: Verify Setup

```bash
# Check directory structure
ls -la src/

# Should see:
# - freebsd/       (FreeBSD source)
# - apple-oss/     (Apple OSS)
# - darwin-compat/ (empty, for ported components)
# - patches/       (empty, for integration patches)

# Review source inventory
cat SOURCE_INVENTORY.md
```

## Troubleshooting

### Git Clone Fails

**Problem**: Timeout or connection errors

**Solution**:

```bash
# Use shallow clone for faster download
git clone --depth 1 https://git.freebsd.org/src.git src/freebsd
```

### Missing Dependencies

**Problem**: `command not found` errors

**Solution**: Manually install missing tools:

```bash
# FreeBSD
pkg install <package-name>

# Linux
apt install <package-name>
```

### Disk Space Issues

**Problem**: Not enough space for source trees

**Solution**:

- Use shallow clones (`--depth 1`)
- Mount additional storage
- Clean up unnecessary files

## Next Steps

Once setup is complete:

1. Review [Architecture Documentation](architecture.md)
2. Read [Porting Guide](porting-guide.md)
3. Start with Phase 2: Code Analysis

## Additional Resources

- [FreeBSD Handbook](https://docs.freebsd.org/en/books/handbook/)
- [FreeBSD Developer's Handbook](https://docs.freebsd.org/en/books/developers-handbook/)
- [Apple Open Source](https://opensource.apple.com/)
- [ravynOS Documentation](https://ravynos.com/docs/)
