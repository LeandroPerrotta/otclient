#pragma once

#if defined(USE_CEF) && defined(_WIN32)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d11.h>
#endif

class WindowsGPUContext
{
public:
    static void initialize();
    static void cleanup();
};

