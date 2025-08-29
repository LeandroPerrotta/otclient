# WebView Rendering Fix for OTClient

## Problem Description

After logging into the game, CEF webviews would disappear or become unresponsive, despite working correctly before login. The following errors appeared in console:

```
ERROR: CefRendererGPULinuxMesa: glCreateMemoryObjectsEXT failed
ERROR: CefRendererGPULinuxMesa: GPU import failed
```

## Root Cause Analysis

The issue was caused by OpenGL context management conflicts between the game's rendering system and CEF's Mesa GPU renderer:

1. **Context Switching**: When the game starts after login, OpenGL context operations interfere with CEF's context expectations
2. **Extension Availability**: The Mesa GPU renderer relies on `GL_EXT_memory_object_fd` extensions that became unavailable after context changes
3. **Threading Issues**: CEF's accelerated paint operations execute in separate threads but expect specific OpenGL context state

## Solution Implemented

### 1. Improved Context Management (`cef_renderergpulinuxmesa.cpp`)

- **Context Preservation**: Save and restore the current OpenGL context before CEF operations
- **Context Validation**: Ensure the correct context is active before GL extension calls
- **Graceful Restoration**: Restore the original context after CEF operations complete

### 2. Enhanced Error Handling

- **Extension Re-initialization**: Attempt to reload GL function pointers if they become null
- **Better Error Reporting**: Include GL error codes in log messages for debugging
- **Clear Error State**: Clear GL error state before attempting operations

### 3. Robust Support Detection (`isSupported()`)

- **Context-aware Checks**: Properly manage context during support detection
- **Complete Restoration**: Ensure original context is restored regardless of detection outcome
- **Detailed Logging**: Log context information for debugging

### 4. Additional Logging (`linuxgpucontext.cpp`)

- Added detailed context information logging during initialization
- Track context state changes for debugging

## Files Modified

1. `src/cef/graphics/gpu/cef_renderergpulinuxmesa.cpp`
   - Enhanced `onAcceleratedPaint()` with proper context management
   - Improved `isSupported()` with context preservation
   - Added extension function re-initialization

2. `src/cef/graphics/gpu/linuxgpucontext.cpp`
   - Added detailed context logging during initialization

## Testing

Use the provided test files to verify the fix:

1. **test_webview_fix.lua** - Lua test script with webview functionality testing
2. **webview_test.otui** - UI definition for the test interface
3. **build_webview_fix.sh** - Build script with proper configuration

### Test Procedure

1. Load a webview before login - should work normally
2. Login to the game completely
3. Test webview functionality again - should continue working
4. Check console for absence of GL error messages

## Fallback Options

If the fix doesn't resolve the issue completely:

1. **Disable GPU Acceleration**: Set `shouldUseSharedTexture()` to `false` in CEF config
2. **Use CPU Renderer**: The system will automatically fall back to `CefRendererCPU`
3. **Configure CEF Settings**: Adjust CEF command-line flags for your specific GPU/driver

## Technical Details

### Context Management Flow

```cpp
// Save current context
GLXContext currentContext = glXGetCurrentContext();
GLXDrawable currentDrawable = glXGetCurrentDrawable();

// Switch to CEF main context for operations
if(currentContext != LinuxGPUContext::mainContext()) {
    glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext());
}

// Perform CEF operations...

// Restore original context
if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
    glXMakeCurrent(x11Display, currentDrawable, currentContext);
}
```

### Extension Function Recovery

```cpp
// Re-initialize if functions become null
if(!m_glCreateMemoryObjectsEXT) {
    m_glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)resolveGLProc("glCreateMemoryObjectsEXT");
}
```

## Compatibility

- **Platform**: Linux with Mesa drivers
- **OpenGL**: Requires `GL_EXT_memory_object_fd` extension support
- **CEF**: Compatible with shared texture rendering
- **Thread Safety**: Maintains thread safety with proper context management

## Future Improvements

1. **Dynamic Context Detection**: Detect context changes and adapt automatically
2. **Performance Monitoring**: Add metrics for context switch performance impact
3. **Driver-specific Optimizations**: Optimize for specific Mesa driver versions
4. **Configuration UI**: Add UI controls for webview rendering options

## Build Instructions

```bash
# Use the provided build script
./build_webview_fix.sh

# Or build manually
mkdir build && cd build
cmake .. -DUSE_CEF=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)
```

## Verification

After applying the fix, webviews should:

- ✅ Work normally before login
- ✅ Continue working after login 
- ✅ Handle context switches gracefully
- ✅ Show no GL errors in console
- ✅ Maintain rendering performance