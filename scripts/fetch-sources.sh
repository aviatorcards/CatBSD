#!/bin/sh
# CatBSD Source Fetching Script
# Downloads FreeBSD and Apple OSS source trees

set -e

echo "=== CatBSD Source Fetching ==="
echo ""

# Load environment if available
if [ -f .catbsd.env ]; then
    . ./.catbsd.env
else
    echo "Warning: .catbsd.env not found. Using defaults."
    CATBSD_ROOT="$(pwd)"
    CATBSD_SRC="$CATBSD_ROOT/src"
    FREEBSD_BRANCH="${FREEBSD_BRANCH:-stable/13}"
    MACOS_VERSION="${MACOS_VERSION:-10.8.5}"
fi

# Fetch FreeBSD source
fetch_freebsd() {
    echo "Fetching FreeBSD source tree..."
    echo "  Branch: $FREEBSD_BRANCH"
    echo ""
    
    FREEBSD_DIR="$CATBSD_SRC/freebsd"
    
    if [ -d "$FREEBSD_DIR/.git" ]; then
        echo "FreeBSD source already exists. Updating..."
        cd "$FREEBSD_DIR"
        git fetch origin
        git checkout "$FREEBSD_BRANCH"
        git pull
        cd "$CATBSD_ROOT"
    else
        echo "Cloning FreeBSD repository..."
        mkdir -p "$CATBSD_SRC"
        git clone --depth 1 --branch "$FREEBSD_BRANCH" \
            https://git.freebsd.org/src.git "$FREEBSD_DIR"
    fi
    
    echo "  ✓ FreeBSD source ready"
}

# Fetch Apple OSS
fetch_apple_oss() {
    echo ""
    echo "Fetching Apple Open Source..."
    echo "  Version: macOS $MACOS_VERSION"
    echo ""
    
    APPLE_DIR="$CATBSD_SRC/apple-oss"
    
    if [ -d "$APPLE_DIR/.git" ]; then
        echo "Apple OSS already exists. Updating..."
        cd "$APPLE_DIR"
        git fetch origin
        git pull
        cd "$CATBSD_ROOT"
    else
        echo "Cloning Apple OSS repository..."
        mkdir -p "$CATBSD_SRC"
        git clone https://github.com/apple-oss-distributions/distribution-macOS.git "$APPLE_DIR"
    fi
    
    # Try to checkout specific version
    cd "$APPLE_DIR"
    
    # List available tags for Mountain Lion (10.8.x)
    echo ""
    echo "Available macOS 10.8 tags:"
    git tag | grep "^macos-1085" || echo "  (No exact 10.8.5 tags found)"
    
    # Checkout closest match or stay on main
    if git tag | grep -q "^macos-1085"; then
        TAG=$(git tag | grep "^macos-1085" | tail -n1)
        echo "Checking out $TAG"
        git checkout "$TAG"
    else
        echo "Warning: No exact 10.8.5 tag found. Staying on default branch."
        echo "You may need to manually checkout the appropriate version."
    fi
    
    cd "$CATBSD_ROOT"
    echo "  ✓ Apple OSS ready"
}

# Verify sources
verify_sources() {
    echo ""
    echo "Verifying source trees..."
    
    FREEBSD_DIR="$CATBSD_SRC/freebsd"
    APPLE_DIR="$CATBSD_SRC/apple-oss"
    
    if [ -d "$FREEBSD_DIR/sys" ]; then
        echo "  ✓ FreeBSD kernel source found"
    else
        echo "  ✗ FreeBSD kernel source missing"
    fi
    
    if [ -d "$FREEBSD_DIR/lib" ]; then
        echo "  ✓ FreeBSD userland libraries found"
    else
        echo "  ✗ FreeBSD userland libraries missing"
    fi
    
    if [ -d "$APPLE_DIR" ]; then
        echo "  ✓ Apple OSS repository found"
        
        # Check for key Darwin components
        if [ -d "$APPLE_DIR/xnu" ] || [ -f "$APPLE_DIR/xnu" ]; then
            echo "  ✓ XNU kernel source available"
        fi
        
        if [ -d "$APPLE_DIR/Libsystem" ] || [ -f "$APPLE_DIR/Libsystem" ]; then
            echo "  ✓ Libsystem available"
        fi
    else
        echo "  ✗ Apple OSS repository missing"
    fi
}

# Create source inventory
create_inventory() {
    echo ""
    echo "Creating source inventory..."
    
    INVENTORY="$CATBSD_ROOT/SOURCE_INVENTORY.md"
    
    cat > "$INVENTORY" << EOF
# CatBSD Source Inventory

Generated: $(date)

## FreeBSD Source

- **Location**: \`src/freebsd/\`
- **Branch**: $FREEBSD_BRANCH
- **Commit**: $(cd "$CATBSD_SRC/freebsd" && git rev-parse --short HEAD 2>/dev/null || echo "unknown")

## Apple OSS Source

- **Location**: \`src/apple-oss/\`
- **Version**: macOS $MACOS_VERSION
- **Commit**: $(cd "$CATBSD_SRC/apple-oss" && git rev-parse --short HEAD 2>/dev/null || echo "unknown")

## Key Components to Port

### High Priority
- [ ] launchd (init system)
- [ ] libdispatch (Grand Central Dispatch)
- [ ] CoreFoundation
- [ ] libsystem

### Medium Priority
- [ ] XNU kernel components (VFS enhancements)
- [ ] IOKit basics
- [ ] Security framework

### Low Priority
- [ ] Additional frameworks as needed

## Notes

- FreeBSD provides the base kernel and userland
- Darwin components will be integrated into \`src/darwin-compat/\`
- Patches for integration will be stored in \`src/patches/\`
EOF
    
    echo "  ✓ Created $INVENTORY"
}

# Main execution
main() {
    fetch_freebsd
    fetch_apple_oss
    verify_sources
    create_inventory
    
    echo ""
    echo "=== Source Fetch Complete ==="
    echo ""
    echo "Source trees are ready in src/"
    echo "Review SOURCE_INVENTORY.md for details"
    echo ""
}

main
