#!/bin/sh
# CatBSD FreeBSD VM - Initial Setup
# Run this inside the FreeBSD VM after first boot

echo "╔════════════════════════════════════════╗"
echo "║  CatBSD FreeBSD Development Setup      ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Update system
echo "=== Updating FreeBSD ==="
sudo freebsd-update fetch install || echo "No updates available"
echo ""

# Install packages
echo "=== Installing development packages ==="
sudo pkg install -y \
    git \
    clang \
    gmake \
    libxml2 \
    rsync \
    vim \
    bash

echo ""
echo "=== Package installation complete ==="
echo ""

# Create CatBSD directory
mkdir -p ~/CatBSD
echo "✓ Created ~/CatBSD directory"
echo ""

echo "╔════════════════════════════════════════╗"
echo "║  Setup Complete!                       ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Next steps:"
echo "  1. From macOS: ./sync-to-vm.sh your-username"
echo "  2. cd ~/CatBSD/src/darwin-compat/shims"
echo "  3. gmake"
echo ""
