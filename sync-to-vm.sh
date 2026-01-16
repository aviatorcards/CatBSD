#!/bin/bash
# CatBSD - Sync source code to FreeBSD VM
# Run this to copy CatBSD source to the VM

set -e

VM_USER="${1:-tristan}"
VM_PORT="2222"
VM_HOST="localhost"

echo "╔════════════════════════════════════════╗"
echo "║  CatBSD Source Sync to FreeBSD VM      ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check if VM is running
if ! nc -z "$VM_HOST" "$VM_PORT" 2>/dev/null; then
    echo "Error: VM not running or SSH not accessible"
    echo "Start VM with: ./run-vm.sh"
    exit 1
fi

echo "Syncing CatBSD source to VM..."
echo "  User: $VM_USER@$VM_HOST:$VM_PORT"
echo ""

# Create remote directory
ssh -p "$VM_PORT" "$VM_USER@$VM_HOST" "mkdir -p ~/CatBSD"

# Sync source (excluding build artifacts and large repos)
rsync -avz --progress \
    --exclude 'vm/' \
    --exclude '*.iso' \
    --exclude 'src/freebsd/' \
    --exclude 'src/apple-oss/' \
    --exclude 'src/darwin-compat/launchd/' \
    --exclude 'src/darwin-compat/libdispatch/' \
    --exclude '*-demo/*-demo' \
    --exclude '*.o' \
    --exclude '.git/' \
    -e "ssh -p $VM_PORT" \
    . "$VM_USER@$VM_HOST:~/CatBSD/"

echo ""
echo "✓ Sync complete!"
echo ""
echo "Next steps:"
echo "  ssh -p $VM_PORT $VM_USER@$VM_HOST"
echo "  cd ~/CatBSD/src/darwin-compat"
echo "  gmake -C shims"
echo ""
