#include "gpu_helper.h"
#include <framework/global.h>
#include <GL/gl.h>
#include <cstring>

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

