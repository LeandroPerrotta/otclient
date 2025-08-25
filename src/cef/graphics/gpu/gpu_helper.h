#pragma once

#if defined(USE_CEF)
#if defined(_WIN32) || defined(__linux__)
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#endif

const char* getEGLErrorString(EGLint error);

