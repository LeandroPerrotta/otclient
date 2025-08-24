#!/bin/bash

# OTClient Pre-Build Script
# This script automates the build process using pre-built cache when available. Specially usefull for automated CI/CD pipelines.

set -e

# Default build parameters
BUILD_PARAMS="-DUSE_CEF=ON"
REBUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-params)
            BUILD_PARAMS="$2"
            shift 2
            ;;
        --rebuild)
            REBUILD=true
            shift
            ;;
        --help|-h)
            echo "OTClient Pre-Build Script"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --build-params PARAMS  CMake build parameters (default: -DUSE_CEF=ON)"
            echo "  --rebuild              Force rebuild by clearing cache"
            echo "  --help, -h             Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                     # Build with default CEF parameters"
            echo "  $0 --rebuild           # Force rebuild (clear cache)"
            echo "  $0 --build-params \"-DUSE_CEF=ON -DCMAKE_BUILD_TYPE=Debug\""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== OTClient Build Script with Intelligent Caching ==="
echo "Build parameters: ${BUILD_PARAMS}"
echo "Rebuild mode: ${REBUILD}"
echo ""

# Ensure pre-build submodule is initialized and up to date
echo "[INFO] Ensuring pre-build submodule is ready..."
if [ -d ".git" ]; then
    if git submodule status pre-build >/dev/null 2>&1; then
        echo "   Updating pre-build submodule..."
        git submodule update --init pre-build
        echo "[OK] Pre-build submodule ready"
    else
        echo "   Warning: pre-build submodule not found, continuing without cache..."
    fi
else
    echo "   Warning: Not a git repository, continuing without submodule update..."
fi
echo ""

# Get current git branch
CURRENT_BRANCH=$(git branch --show-current 2>/dev/null || echo "master")
if [ -z "$CURRENT_BRANCH" ]; then
    CURRENT_BRANCH="master"
fi

# Set build destination (Linux only)
BUILD_DEST="./pre-build/otclient/${CURRENT_BRANCH}/ubuntu-24"

echo "Current branch: ${CURRENT_BRANCH}"
echo "Platform: ubuntu-24"
echo "Build destination: ${BUILD_DEST}"
echo ""

# Handle rebuild flag
if [ "$REBUILD" = true ]; then
    echo "[INFO] Rebuild mode enabled, clearing cache..."
    if [ -d "$BUILD_DEST" ]; then
        rm -rf "$BUILD_DEST"
        echo "   Cache cleared: $BUILD_DEST"
    else
        echo "   No cache to clear"
    fi
    echo ""
fi

# Check if build destination exists and has content
BUILD_READY=false
if [ -d "$BUILD_DEST" ] && [ "$(ls -A "$BUILD_DEST" 2>/dev/null)" ]; then
    echo "[INFO] Build cache found and not empty"
    echo "   Location: $BUILD_DEST"
    
    # Check for essential build files
    if [ -f "$BUILD_DEST/CMakeCache.txt" ] && [ -f "$BUILD_DEST/Makefile" ]; then
        BUILD_READY=true
        echo "   Build files ready: CMakeCache.txt, Makefile"
    else
        echo "   Build files incomplete, will reconfigure"
    fi
else
    echo "[INFO] Build cache not found or empty"
fi

# Create build directory and configure if needed
if [ "$BUILD_READY" = false ]; then
    echo "[INFO] Setting up build environment..."
    mkdir -p "$BUILD_DEST"
    
    echo "   Configuring CMake..."
    echo "   Command: cmake -S . -B $BUILD_DEST ${BUILD_PARAMS}"
    
    if cmake -S . -B "$BUILD_DEST" ${BUILD_PARAMS}; then
        echo "[OK] CMake configuration successful"
        BUILD_READY=true
    else
        echo "[ERROR] CMake configuration failed"
        exit 1
    fi
fi

# Build the project
if [ "$BUILD_READY" = true ]; then
    echo "[INFO] Building project..."
    echo "   Command: cmake --build $BUILD_DEST"
    
    if cmake --build "$BUILD_DEST"; then
        echo "[OK] Build successful!"
        
        # Copy executables to project root
        echo "[INFO] Copying executables to project root..."
        
        if [ -f "$BUILD_DEST/otclient" ]; then
            cp "$BUILD_DEST/otclient" .
            echo "   otclient copied to project root"
        else
            echo "   Warning: otclient executable not found"
        fi
        
        if [ -f "$BUILD_DEST/otclient_cef_subproc" ]; then
            # Create cef directory if it doesn't exist
            mkdir -p "./cef"
            cp "$BUILD_DEST/otclient_cef_subproc" "./cef/"
            echo "   otclient_cef_subproc copied to ./cef/"
        else
            echo "   Warning: otclient_cef_subproc executable not found"
        fi
        
        echo ""
        echo "[SUCCESS] Build completed successfully!"
        echo "   Main executable: ./otclient"
        echo "   CEF subprocess: ./cef/otclient_cef_subproc"
        echo "   Cache location: $BUILD_DEST"
        
    else
        echo "[ERROR] Build failed"
        exit 1
    fi
else
    echo "[ERROR] Build environment not ready"
    exit 1
fi
