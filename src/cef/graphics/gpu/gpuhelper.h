#pragma once

#if defined(USE_CEF)
#if defined(_WIN32) || defined(__linux__)
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#endif

const char* getEGLErrorString(EGLint error);
bool isMesaDriver();

#if defined(USE_CEF) && defined(_WIN32)
void logD3D11DeviceInfo();
void logEGLInfo();
#endif

