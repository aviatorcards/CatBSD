#!/bin/bash
# CatBSD FreeBSD VM - Daily Runner
# Use this after installation is complete

set -e

# Configuration
VM_NAME="catbsd-freebsd"
VM_DIR="vm"
MEMORY="4G"
CPUS="4"
DISK_IMG="$VM_DIR/${VM_NAME}.qcow2"

# Shared folder (CatBSD source)
SHARED_DIR="$(pwd)"

# Check if disk exists
if [ ! -f "$DISK_IMG" ]; then
    echo "Error: VM disk not found!"
    echo "Run ./install-vm.sh first to install FreeBSD"
    exit 1
fi

echo "╔════════════════════════════════════════╗"
echo "║  CatBSD FreeBSD VM - Development       ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Starting VM..."
echo ""
echo "Access:"
echo "  SSH:     ssh -p 2222 user@localhost"
echo "  Console: Use QEMU window"
echo ""
echo "Shared folder will be available via SSH/SCP"
echo "  scp -P 2222 file user@localhost:~/"
echo ""

# Run QEMU
qemu-system-x86_64 \
    -name "$VM_NAME" \
    -machine type=q35,accel=tcg \
    -cpu qemu64 \
    -smp "$CPUS" \
    -m "$MEMORY" \
    -drive file="$DISK_IMG",if=virtio,format=qcow2 \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -display cocoa \
    -vga virtio \
    -monitor stdio

echo ""
echo "VM stopped."
