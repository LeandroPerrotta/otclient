#include "gpuhelper.h"
#include <framework/core/logger.h>
#include <framework/stdext/format.h>
#include <GL/gl.h>
#include <cstring>
#if defined(USE_CEF) && defined(_WIN32)
#include <d3d11.h>
#include <dxgi.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

const char* getEGLErrorString(EGLint error)
{
#if defined(USE_CEF) && (defined(_WIN32) || defined(__linux__))
    switch(error) {
    case EGL_SUCCESS:
        return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
        return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
        return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
        return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
        return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
        return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
        return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
        return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
        return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
        return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
        return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
        return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
        return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
        return "EGL_BAD_NATIVE_WINDOW";
    case EGL_CONTEXT_LOST:
        return "EGL_CONTEXT_LOST";
    default:
        return "Unknown EGL error";
    }
#else
    (void)error;
    return "EGL not supported";
#endif
}

bool isMesaDriver()
{
#if defined(USE_CEF) && defined(__linux__)
    const char* vendorStr = (const char*)glGetString(GL_VENDOR);
    const char* rendererStr = (const char*)glGetString(GL_RENDERER);
    const char* versionStr = (const char*)glGetString(GL_VERSION);
    static bool logged = false;
    if(!logged) {
        g_logger.info(stdext::format("CefRendererGPULinux: vendor: %s, renderer: %s, version: %s",
                                     vendorStr ? vendorStr : "?",
                                     rendererStr ? rendererStr : "?",
                                     versionStr ? versionStr : "?"));
        logged = true;
    }
    return (vendorStr && strstr(vendorStr, "Mesa")) ||
           (rendererStr && (strstr(rendererStr, "Gallium") || strstr(rendererStr, "Mesa"))) ||
           (versionStr && strstr(versionStr, "Mesa"));
#else
    return false;
#endif
}

#if defined(USE_CEF) && defined(_WIN32)
void logD3D11DeviceInfo()
{
    static bool logged = false;
    if (logged) return;
    logged = true;

    // Create a temporary D3D11 device to get system info
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context
    );
    
    if (SUCCEEDED(hr)) {
        // Get DXGI adapter info
        IDXGIDevice* dxgiDevice = nullptr;
        if (SUCCEEDED(device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(adapter->GetDesc(&desc))) {
                    char adapterName[256];
                    wcstombs(adapterName, desc.Description, sizeof(adapterName));
                    g_logger.info(stdext::format("D3D11 Adapter: %s", adapterName));
                    g_logger.info(stdext::format("D3D11 Video Memory: %u MB", 
                                                desc.DedicatedVideoMemory / (1024 * 1024)));
                }
                adapter->Release();
            }
            dxgiDevice->Release();
        }
        
        g_logger.info(stdext::format("D3D11 Feature Level: 0x%x", featureLevel));
        
        context->Release();
        device->Release();
    } else {
        g_logger.error(stdext::format("Failed to create D3D11 device for diagnostics: 0x%x", hr));
    }
}

void logEGLInfo()
{
    static bool logged = false;
    if (logged) return;
    logged = true;

    EGLDisplay display = eglGetCurrentDisplay();
    if (display != EGL_NO_DISPLAY) {
        const char* vendor = eglQueryString(display, EGL_VENDOR);
        const char* version = eglQueryString(display, EGL_VERSION);
        const char* clientAPIs = eglQueryString(display, EGL_CLIENT_APIS);
        
        g_logger.info(stdext::format("EGL Vendor: %s", vendor ? vendor : "unknown"));
        g_logger.info(stdext::format("EGL Version: %s", version ? version : "unknown"));
        g_logger.info(stdext::format("EGL Client APIs: %s", clientAPIs ? clientAPIs : "unknown"));
        
        // Check for ANGLE
        if (vendor && strstr(vendor, "Google Inc.")) {
            g_logger.info("ANGLE renderer detected");
        }
    } else {
        g_logger.error("No current EGL display available");
    }
}
#endif

