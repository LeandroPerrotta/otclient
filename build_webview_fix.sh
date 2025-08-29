#!/bin/bash

# Build script for OTClient with webview fixes
# This script helps rebuild the CEF components with the applied fixes

echo "Building OTClient with WebView fixes..."
echo "========================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the OTClient root directory."
    exit 1
fi

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

echo "Configuring CMake with CEF support..."
cmake .. -DUSE_CEF=ON \
         -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DCMAKE_CXX_FLAGS="-O2 -g" \
         -DVERBOSE=ON

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed!"
    exit 1
fi

echo "Building OTClient..."
echo "Note: This may take a while, especially on first build..."

# Build with parallel jobs (use number of CPU cores)
JOBS=$(nproc)
echo "Using $JOBS parallel jobs..."

make -j$JOBS

if [ $? -ne 0 ]; then
    echo "Error: Build failed!"
    echo ""
    echo "Common solutions:"
    echo "1. Make sure all dependencies are installed"
    echo "2. Check if CEF libraries are properly installed (run setup_cef.sh)"
    echo "3. Try building with fewer jobs: make -j1"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo ""
echo "To test the webview fix:"
echo "1. Copy test_webview_fix.lua to your modules directory"
echo "2. Copy webview_test.otui to your modules directory"  
echo "3. Load the test module in your init.lua"
echo "4. Run the client and test webviews before and after login"
echo ""
echo "The fix includes:"
echo "- Improved OpenGL context management in CEF Mesa renderer"
echo "- Better error handling and recovery for GL extension failures"
echo "- Context restoration to prevent interference with game rendering"
echo "- Additional logging for debugging"
echo ""
echo "If webviews still don't work after login, you can:"
echo "1. Disable GPU acceleration in CEF config (shouldUseSharedTexture = false)"
echo "2. Use CPU rendering fallback"
echo "3. Check console output for specific GL errors"