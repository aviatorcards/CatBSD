#!/bin/bash
# CatBSD - Simple sync using tar+scp (no rsync needed)

set -e

VM_USER="${1:-tristan}"
VM_PORT="2222"
VM_HOST="localhost"

echo "╔════════════════════════════════════════╗"
echo "║  CatBSD Source Sync (tar method)       ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check if VM is running
if ! nc -z "$VM_HOST" "$VM_PORT" 2>/dev/null; then
    echo "Error: VM not running or SSH not accessible"
    exit 1
fi

echo "Creating archive..."
tar czf /tmp/catbsd-sync.tar.gz \
    --exclude='vm/' \
    --exclude='*.iso' \
    --exclude='src/freebsd/' \
    --exclude='src/apple-oss/' \
    --exclude='src/darwin-compat/launchd/' \
    --exclude='src/darwin-compat/libdispatch/' \
    --exclude='*-demo/*-demo' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='.git/' \
    .

echo "Copying to VM..."
scp -P "$VM_PORT" /tmp/catbsd-sync.tar.gz "$VM_USER@$VM_HOST:~/"

echo "Extracting on VM..."
ssh -p "$VM_PORT" "$VM_USER@$VM_HOST" "mkdir -p ~/CatBSD && cd ~/CatBSD && tar xzf ~/catbsd-sync.tar.gz && rm ~/catbsd-sync.tar.gz"

rm /tmp/catbsd-sync.tar.gz

echo ""
echo "✓ Sync complete!"
echo ""
