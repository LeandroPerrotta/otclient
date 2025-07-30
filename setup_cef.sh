#!/bin/bash

# OTClient CEF Setup Script
# This script downloads, extracts, and compiles CEF for OTClient

set -e

CEF_VERSION="103.0.12+g8eb56c7+chromium-103.0.5060.134"
CEF_URL="https://cef-builds.spotifycdn.com/cef_binary_${CEF_VERSION}_linux64_minimal.tar.bz2"
CEF_ARCHIVE="/tmp/cef_binary_${CEF_VERSION}_linux64_minimal.tar.bz2"

# Default to local development mode
INSTALL_MODE="local"
CEF_RUNTIME_DIR="cef"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --install|--global)
            INSTALL_MODE="global"
            CEF_RUNTIME_DIR="/opt/otclient-cef"
            shift
            ;;
        --user)
            INSTALL_MODE="user"
            CEF_RUNTIME_DIR="$HOME/.local/share/otclient-cef"
            shift
            ;;
        --help|-h)
            echo "OTClient CEF Setup Script"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  (no option)     Install locally for development (./cef/)"
            echo "  --install       Install globally for all users (/opt/otclient-cef/)"
            echo "  --user          Install for current user (~/.local/share/otclient-cef/)"
            echo "  --help, -h      Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0              # Local development"
            echo "  $0 --install    # Global installation (requires sudo)"
            echo "  $0 --user       # User installation"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== OTClient CEF Setup Script ==="
echo "Version: ${CEF_VERSION}"
echo "Install mode: ${INSTALL_MODE}"
echo "Runtime directory: ${CEF_RUNTIME_DIR}"
echo ""

# Check if CEF runtime is ready
if [ -d "$CEF_RUNTIME_DIR" ] && [ -f "$CEF_RUNTIME_DIR/libcef.so" ] && [ -f "$CEF_RUNTIME_DIR/libcef_dll_wrapper.a" ]; then
    echo "✅ CEF runtime already exists and is ready"
    echo "   Location: $CEF_RUNTIME_DIR"
    echo "   Libraries: $CEF_RUNTIME_DIR/libcef.so, $CEF_RUNTIME_DIR/libcef_dll_wrapper.a"
    
    # Always copy to local cef/ for distribution
    echo "📋 Copying runtime files to ./cef/ for distribution..."
    mkdir -p "./cef"
    cp "$CEF_RUNTIME_DIR"/* "./cef/" 2>/dev/null || true
    if [ -d "$CEF_RUNTIME_DIR/locales" ]; then
        cp -r "$CEF_RUNTIME_DIR/locales" "./cef/"
        echo "✅ Locales directory copied to ./cef/"
    fi
    echo "✅ Distribution files ready in ./cef/"
    
    exit 0
fi

# Check for required tools
if ! command -v wget &> /dev/null; then
    echo "❌ Error: wget is required but not installed"
    echo "   Please install wget: sudo apt install wget"
    exit 1
fi

if ! command -v tar &> /dev/null; then
    echo "❌ Error: tar is required but not installed"
    echo "   Please install tar: sudo apt install tar"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "❌ Error: cmake is required but not installed"
    echo "   Please install cmake: sudo apt install cmake"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "❌ Error: make is required but not installed"
    echo "   Please install make: sudo apt install make"
    exit 1
fi

# Check permissions for global installation
if [ "$INSTALL_MODE" = "global" ]; then
    if [ "$EUID" -ne 0 ]; then
        echo "❌ Error: Global installation requires sudo privileges"
        echo "   Please run: sudo $0 --install"
        exit 1
    fi
fi

# Download CEF if not exists
if [ ! -f "$CEF_ARCHIVE" ]; then
    echo "📥 Downloading CEF to /tmp (this may take a few minutes)..."
    wget --progress=bar:force "$CEF_URL" -O "$CEF_ARCHIVE"
    
    if [ ! -f "$CEF_ARCHIVE" ]; then
        echo "❌ Error: Failed to download CEF"
        exit 1
    fi
    echo "✅ CEF downloaded successfully"
else
    echo "✅ CEF archive already exists in /tmp"
fi

# Extract CEF to runtime directory
echo "📦 Extracting CEF to runtime directory..."
mkdir -p "$CEF_RUNTIME_DIR"
cd "$CEF_RUNTIME_DIR"
tar -xf "$CEF_ARCHIVE"

# Find the extracted directory
CEF_EXTRACTED_DIR=$(find . -maxdepth 1 -type d -name "cef_binary_*" | head -1)
if [ -z "$CEF_EXTRACTED_DIR" ]; then
    echo "❌ Error: Failed to extract CEF"
    exit 1
fi

# Move contents to runtime directory
echo "📋 Moving CEF files to runtime directory..."
mv "$CEF_EXTRACTED_DIR"/* .
rmdir "$CEF_EXTRACTED_DIR"

echo "🧹 Cleaning up download..."
rm -f "$CEF_ARCHIVE"
echo "✅ CEF extracted to runtime directory"

cd - > /dev/null

# Compile CEF wrapper if not exists
if [ ! -f "$CEF_RUNTIME_DIR/Release/libcef_dll_wrapper.a" ]; then
    echo "🔨 Compiling CEF wrapper library..."
    
    # Go to CEF directory
    cd "$CEF_RUNTIME_DIR"
    
    # Configure CEF
    echo "   Configuring CEF..."
    cmake -DCMAKE_BUILD_TYPE=Release .
    
    if [ $? -eq 0 ]; then
        # Compile wrapper
        echo "   Compiling wrapper..."
        make libcef_dll_wrapper -j4
        
        if [ $? -eq 0 ]; then
            echo "✅ CEF wrapper compiled successfully"
            
            # Move wrapper to Release directory if it was created elsewhere
            if [ -f "libcef_dll_wrapper/libcef_dll_wrapper.a" ]; then
                echo "   Moving wrapper to Release directory..."
                cp libcef_dll_wrapper/libcef_dll_wrapper.a Release/
                echo "✅ Wrapper moved to Release directory"
            fi
        else
            echo "❌ Error: Failed to compile CEF wrapper"
            exit 1
        fi
    else
        echo "❌ Error: Failed to configure CEF"
        exit 1
    fi
    
    cd - > /dev/null
else
    echo "✅ CEF wrapper already exists"
fi

# Verify all necessary files are in place
echo "📋 Verifying CEF runtime files..."
if [ ! -d "$CEF_RUNTIME_DIR/Release" ]; then
    echo "❌ Error: Release directory not found"
    exit 1
fi

if [ ! -d "$CEF_RUNTIME_DIR/include" ]; then
    echo "❌ Error: Include directory not found"
    exit 1
fi

if [ ! -d "$CEF_RUNTIME_DIR/Resources" ]; then
    echo "❌ Error: Resources directory not found"
    exit 1
fi

echo "✅ CEF runtime files verified"

# Set proper permissions for global installation
if [ "$INSTALL_MODE" = "global" ]; then
    echo "🔐 Setting permissions for global installation..."
    chmod 755 "$CEF_RUNTIME_DIR"
    chmod 644 "$CEF_RUNTIME_DIR"/*.a
    chmod 755 "$CEF_RUNTIME_DIR"/*.so
    chmod 755 "$CEF_RUNTIME_DIR"/chrome-sandbox
    echo "✅ Permissions set"
fi

echo "✅ CEF runtime files copied to $CEF_RUNTIME_DIR"

# Always copy to local cef/ for distribution (RUNTIME ONLY, no includes)
echo "📋 Copying runtime files to ./cef/ for distribution..."
mkdir -p "./cef"
cp "$CEF_RUNTIME_DIR/Release/"* "./cef/" 2>/dev/null || true
cp "$CEF_RUNTIME_DIR/Resources/"* "./cef/" 2>/dev/null || true
if [ -d "$CEF_RUNTIME_DIR/Resources/locales" ]; then
    cp -r "$CEF_RUNTIME_DIR/Resources/locales" "./cef/"
    echo "✅ Locales directory copied to ./cef/"
fi
echo "✅ Distribution files ready in ./cef/ (runtime only)"

echo ""
echo "🎯 CEF setup completed successfully!"
echo "   Install mode: ${INSTALL_MODE}"
echo "   Runtime location: $CEF_RUNTIME_DIR"
echo "   Libraries: $CEF_RUNTIME_DIR/libcef.so, $CEF_RUNTIME_DIR/libcef_dll_wrapper.a"
echo "   Distribution: ./cef/"
echo ""
echo "📝 You can now compile OTClient with CEF support:"
echo "   mkdir build && cd build"
echo "   cmake -DUSE_CEF=ON .."
echo "   make -j$(nproc)"
echo ""
if [ "$INSTALL_MODE" = "global" ]; then
    echo "💡 Global installation complete! Other users can now compile OTClient with CEF."
elif [ "$INSTALL_MODE" = "user" ]; then
    echo "💡 User installation complete! CEF is available for your user account."
else
    echo "💡 Local development setup complete! CEF is ready for development."
fi
echo "💡 Distribution files are ready in ./cef/" 