#!/bin/sh
# CatBSD Darwin Component Import Script
# Imports and prepares Darwin components for integration

set -e

echo "=== CatBSD Darwin Component Import ==="
echo ""

# Load environment
if [ -f .catbsd.env ]; then
    . ./.catbsd.env
else
    CATBSD_ROOT="$(pwd)"
    CATBSD_SRC="$CATBSD_ROOT/src"
fi

APPLE_SRC="$CATBSD_SRC/apple-oss"
DARWIN_COMPAT="$CATBSD_SRC/darwin-compat"
MANIFEST="$DARWIN_COMPAT/MANIFEST.md"

# Component to import (passed as argument)
COMPONENT=$1

if [ -z "$COMPONENT" ]; then
    echo "Usage: $0 <component>"
    echo ""
    echo "Available components:"
    echo "  blocks       - Blocks runtime (from compiler-rt)"
    echo "  libdispatch  - Grand Central Dispatch"
    echo "  CoreFoundation - Core framework"
    echo "  libsystem    - System library shims"
    echo "  launchd      - Init system"
    echo ""
    exit 1
fi

# Create darwin-compat structure
mkdir -p "$DARWIN_COMPAT"
mkdir -p "$DARWIN_COMPAT/patches"

# Initialize manifest if it doesn't exist
if [ ! -f "$MANIFEST" ]; then
    cat > "$MANIFEST" << 'EOF'
# Darwin Component Manifest

This file tracks all Darwin components imported into CatBSD.

## Imported Components

| Component | Version | Source | Import Date | Status |
|-----------|---------|--------|-------------|--------|
EOF
fi

# Import component
import_component() {
    local comp=$1
    local source_path="$APPLE_SRC/$comp"
    local dest_path="$DARWIN_COMPAT/$comp"
    
    echo "Importing $comp..."
    
    # Check if source exists
    if [ ! -d "$source_path" ] && [ ! -f "$source_path" ]; then
        echo "  ✗ Source not found: $source_path"
        echo "  Checking alternative locations..."
        
        # Try to find it
        if [ -d "$APPLE_SRC" ]; then
            find "$APPLE_SRC" -maxdepth 2 -name "$comp" -type d 2>/dev/null | head -n 1
        fi
        
        return 1
    fi
    
    # Copy component
    echo "  Copying from $source_path..."
    cp -R "$source_path" "$dest_path"
    
    # Create patches directory for this component
    mkdir -p "$DARWIN_COMPAT/patches/$comp"
    
    # Add to manifest
    echo "| $comp | - | apple-oss/$comp | $(date +%Y-%m-%d) | Imported |" >> "$MANIFEST"
    
    echo "  ✓ Imported $comp to $dest_path"
    
    # Create component README
    cat > "$dest_path/README.catbsd.md" << COMPEOF
# $comp - CatBSD Port

**Import Date**: $(date)
**Source**: apple-oss-distributions/$comp
**Status**: Imported, not yet integrated

## Build Status

- [ ] Compiles on FreeBSD
- [ ] Passes unit tests
- [ ] Integrated into build system
- [ ] Documentation updated

## Patches Applied

None yet. Patches will be stored in \`../patches/$comp/\`

## Build Instructions

TODO: Add build instructions after initial integration

## Notes

TODO: Add porting notes, known issues, etc.
COMPEOF
    
    echo "  ✓ Created README.catbsd.md"
}

# Import based on component name
case $COMPONENT in
    blocks)
        echo "Note: Blocks runtime should be obtained from LLVM compiler-rt"
        echo "      Not from Apple OSS directly"
        echo ""
        echo "Run: git clone https://github.com/llvm/llvm-project.git"
        echo "     cp -R llvm-project/compiler-rt/lib/BlocksRuntime $DARWIN_COMPAT/blocks"
        ;;
    
    libdispatch)
        import_component "libdispatch"
        echo ""
        echo "Next steps for libdispatch:"
        echo "  1. Review dependencies (pthreads, kqueue, blocks)"
        echo "  2. Check for FreeBSD-specific patches needed"
        echo "  3. Set up build system integration"
        ;;
    
    CoreFoundation)
        import_component "CoreFoundation"
        echo ""
        echo "Next steps for CoreFoundation:"
        echo "  1. Review dependencies (libdispatch, ICU)"
        echo "  2. Identify macOS-specific code to stub out"
        echo "  3. Set up build system integration"
        ;;
    
    libsystem)
        import_component "Libsystem"
        echo ""
        echo "Next steps for Libsystem:"
        echo "  1. Create shim library structure"
        echo "  2. Map to FreeBSD equivalents"
        echo "  3. Implement missing components"
        ;;
    
    launchd)
        import_component "launchd"
        echo ""
        echo "Next steps for launchd:"
        echo "  1. Review XPC dependencies"
        echo "  2. Plan Mach IPC emulation strategy"
        echo "  3. Consider simplified implementation"
        ;;
    
    *)
        echo "Unknown component: $COMPONENT"
        echo "Attempting generic import..."
        import_component "$COMPONENT"
        ;;
esac

echo ""
echo "=== Import Complete ==="
echo ""
echo "Component imported to: $DARWIN_COMPAT/$COMPONENT"
echo "Manifest updated: $MANIFEST"
echo ""
