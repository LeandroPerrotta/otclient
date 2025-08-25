#pragma once

#include <mutex>

#if defined(USE_CEF) && defined(__linux__)
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#endif

class LinuxGPUContext
{
public:
    static void initialize();
    static void cleanup();

#if defined(USE_CEF) && defined(__linux__)
    static Display* x11Display();
    static GLXContext sharedContext();
    static GLXContext mainContext();
    static GLXDrawable drawable();
    static EGLDisplay eglDisplay();
    static bool eglSidecarReady();
#endif
};

