#!/bin/bash
# CatBSD FreeBSD VM - Installation Script
# Run this once to install FreeBSD, then use run-vm.sh for daily use

set -e

# Configuration
VM_NAME="catbsd-freebsd"
VM_DIR="vm"
DISK_SIZE="20G"
MEMORY="4G"
CPUS="4"
ISO_FILE="FreeBSD-14.3-RELEASE-amd64-disc1.iso"

# Find ISO (check common locations)
if [ -f "$ISO_FILE" ]; then
    ISO_PATH="$ISO_FILE"
elif [ -f "../$ISO_FILE" ]; then
    ISO_PATH="../$ISO_FILE"
else
    echo "Error: FreeBSD ISO not found!"
    echo "Expected: $ISO_FILE"
    echo "Please download from: https://www.freebsd.org/where/"
    exit 1
fi

echo "╔════════════════════════════════════════╗"
echo "║  CatBSD FreeBSD VM - Installation     ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Configuration:"
echo "  Name:     $VM_NAME"
echo "  Disk:     $DISK_SIZE"
echo "  Memory:   $MEMORY"
echo "  CPUs:     $CPUS"
echo "  ISO:      $ISO_PATH"
echo ""

# Create VM directory
mkdir -p "$VM_DIR"

# Create disk image if it doesn't exist
DISK_IMG="$VM_DIR/${VM_NAME}.qcow2"
if [ ! -f "$DISK_IMG" ]; then
    echo "Creating virtual disk ($DISK_SIZE)..."
    qemu-img create -f qcow2 "$DISK_IMG" "$DISK_SIZE"
    echo "✓ Disk created"
else
    echo "⚠️  Disk already exists: $DISK_IMG"
    read -p "Delete and recreate? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm "$DISK_IMG"
        qemu-img create -f qcow2 "$DISK_IMG" "$DISK_SIZE"
        echo "✓ Disk recreated"
    fi
fi

echo ""
echo "Starting FreeBSD installation..."
echo ""
echo "Installation Tips:"
echo "  1. Choose 'Install' at boot menu"
echo "  2. Hostname: catbsd-dev"
echo "  3. Partitioning: Auto (ZFS) recommended"
echo "  4. Enable: sshd, ntpd, dumpdev"
echo "  5. Add user: your username (add to wheel group)"
echo "  6. After install, shutdown VM and run ./run-vm.sh"
echo ""
echo "Press Enter to start installation..."
read

# Run QEMU with installation ISO
qemu-system-x86_64 \
    -name "$VM_NAME" \
    -machine type=q35,accel=tcg \
    -cpu qemu64 \
    -smp "$CPUS" \
    -m "$MEMORY" \
    -drive file="$DISK_IMG",if=virtio,format=qcow2 \
    -cdrom "$ISO_PATH" \
    -boot d \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -display cocoa \
    -vga virtio

echo ""
echo "Installation complete!"
echo ""
echo "Next steps:"
echo "  1. Run: ./run-vm.sh"
echo "  2. SSH: ssh -p 2222 user@localhost"
echo "  3. Install packages: pkg install git clang gmake"
echo ""
