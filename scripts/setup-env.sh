#!/bin/sh
# CatBSD Environment Setup Script
# Detects host OS, installs dependencies, and configures build environment

set -e

echo "=== CatBSD Environment Setup ==="
echo ""

# Detect host OS
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
    elif [ "$(uname)" = "FreeBSD" ]; then
        OS="freebsd"
    elif [ "$(uname)" = "Darwin" ]; then
        OS="macos"
    else
        OS="unknown"
    fi
    echo "Detected OS: $OS"
}

# Install dependencies based on OS
install_dependencies() {
    echo ""
    echo "Installing dependencies..."
    
    case $OS in
        freebsd)
            echo "Using FreeBSD pkg..."
            sudo pkg install -y git subversion clang llvm cmake ninja python3 gmake
            ;;
        ubuntu|debian)
            echo "Using apt..."
            sudo apt update
            sudo apt install -y git subversion clang llvm cmake ninja-build python3 build-essential
            ;;
        fedora|rhel|centos)
            echo "Using dnf/yum..."
            sudo dnf install -y git subversion clang llvm cmake ninja-build python3 make gcc
            ;;
        macos)
            echo "Using Homebrew..."
            if ! command -v brew >/dev/null 2>&1; then
                echo "Error: Homebrew not found. Please install from https://brew.sh"
                exit 1
            fi
            brew install git subversion llvm cmake ninja python3
            ;;
        *)
            echo "Warning: Unknown OS. Please install dependencies manually:"
            echo "  - git, subversion"
            echo "  - clang, llvm"
            echo "  - cmake, ninja"
            echo "  - python3"
            echo "  - make/gmake"
            ;;
    esac
}

# Verify toolchain
verify_toolchain() {
    echo ""
    echo "Verifying toolchain..."
    
    MISSING=""
    
    for cmd in git clang cmake python3; do
        if ! command -v $cmd >/dev/null 2>&1; then
            MISSING="$MISSING $cmd"
        else
            VERSION=$($cmd --version 2>&1 | head -n1)
            echo "  ✓ $cmd: $VERSION"
        fi
    done
    
    if [ -n "$MISSING" ]; then
        echo "Error: Missing required tools:$MISSING"
        exit 1
    fi
}

# Set up build directories
setup_directories() {
    echo ""
    echo "Setting up build directories..."
    
    mkdir -p build/obj
    mkdir -p build/release
    mkdir -p src/freebsd
    mkdir -p src/darwin-compat
    mkdir -p src/patches
    mkdir -p src/kernel
    mkdir -p src/userland
    
    echo "  ✓ Created build directories"
}

# Configure environment variables
setup_environment() {
    echo ""
    echo "Configuring environment..."
    
    ENV_FILE=".catbsd.env"
    cat > $ENV_FILE << 'EOF'
# CatBSD Build Environment
export CATBSD_ROOT="$(pwd)"
export CATBSD_BUILD="$CATBSD_ROOT/build"
export CATBSD_SRC="$CATBSD_ROOT/src"
export CATBSD_FREEBSD="$CATBSD_SRC/freebsd"
export CATBSD_DARWIN="$CATBSD_SRC/darwin-compat"

# Build configuration
export MACOS_VERSION="10.8.5"
export FREEBSD_BRANCH="stable/13"

# Compiler settings
export CC=clang
export CXX=clang++
export MAKE=make

# Add to PATH if needed
# export PATH="$CATBSD_ROOT/scripts:$PATH"
EOF
    
    echo "  ✓ Created $ENV_FILE"
    echo ""
    echo "To activate the environment, run:"
    echo "  source $ENV_FILE"
}

# Main execution
main() {
    detect_os
    
    echo ""
    read -p "Install dependencies? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_dependencies
    fi
    
    verify_toolchain
    setup_directories
    setup_environment
    
    echo ""
    echo "=== Setup Complete ==="
    echo ""
    echo "Next steps:"
    echo "  1. source .catbsd.env"
    echo "  2. ./scripts/fetch-sources.sh"
    echo ""
}

main
