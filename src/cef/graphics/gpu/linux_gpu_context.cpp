#include "linux_gpu_context.h"
#include <framework/core/logger.h>
#include <cstring>
#include "gpu_helper.h"
#include <thread>
#if defined(__linux__)
#include <X11/X.h>
#endif

#if defined(USE_CEF) && defined(__linux__)
namespace {
bool g_glxInitialized = false;
Display* g_x11Display = nullptr;
GLXContext g_sharedContext = nullptr;
GLXContext g_mainContext = nullptr;
GLXDrawable g_drawable = 0;
GLXPbuffer g_pbuffer = 0;
bool g_eglInitialized = false;
EGLDisplay g_eglDisplay = EGL_NO_DISPLAY;
EGLContext g_eglContext = EGL_NO_CONTEXT;
}

void LinuxGPUContext::initialize()
{
    if(g_glxInitialized)
        return;

    g_logger.info("LinuxGPUContext: Initializing GLX shared context");
    Display* x11Display = glXGetCurrentDisplay();
    if(!x11Display) {
        g_logger.error("LinuxGPUContext: No current GLX display");
        return;
    }
    g_x11Display = x11Display;

    GLXContext mainCtx = glXGetCurrentContext();
    GLXDrawable mainDrawable = glXGetCurrentDrawable();
    if(!mainCtx || !mainDrawable) {
        g_logger.error("LinuxGPUContext: No main GLX context or drawable");
        return;
    }
    g_mainContext = mainCtx;
    g_drawable = mainDrawable;

    int fbConfigAttribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };

    int numConfigs;
    GLXFBConfig* fbConfigs = glXChooseFBConfig(x11Display, DefaultScreen(x11Display), fbConfigAttribs, &numConfigs);
    if(!fbConfigs || numConfigs == 0) {
        g_logger.error("LinuxGPUContext: Failed to choose GLX FB config");
        return;
    }
    GLXFBConfig fbConfig = fbConfigs[0];
    XFree(fbConfigs);

    int pbufferAttribs[] = {
        GLX_PBUFFER_WIDTH, 1,
        GLX_PBUFFER_HEIGHT, 1,
        None
    };
    g_pbuffer = glXCreatePbuffer(x11Display, fbConfig, pbufferAttribs);
    if(!g_pbuffer) {
        g_logger.error("LinuxGPUContext: Failed to create GLX Pbuffer");
        return;
    }

    g_sharedContext = glXCreateNewContext(x11Display, fbConfig, GLX_RGBA_TYPE, mainCtx, True);
    if(!g_sharedContext) {
        g_logger.error("LinuxGPUContext: Failed to create shared GLX context");
        return;
    }
    g_glxInitialized = true;

    g_logger.info("LinuxGPUContext: GLX shared context created");

    if(g_eglInitialized)
        return;

    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(eglDisplay == EGL_NO_DISPLAY)
        return;
    EGLint major, minor;
    if(!eglInitialize(eglDisplay, &major, &minor))
        return;
    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    if(!extensions ||
       !strstr(extensions, "EGL_EXT_image_dma_buf_import") ||
       !strstr(extensions, "EGL_KHR_image_base") ||
       !strstr(extensions, "EGL_KHR_gl_texture_2D_image")) {
        return;
    }
    g_logger.info("LinuxGPUContext: Required EGL extensions found");
    g_eglDisplay = eglDisplay;
    g_eglContext = EGL_NO_CONTEXT;
    g_eglInitialized = true;
    g_logger.info("LinuxGPUContext: EGL sidecar initialized");
}

void LinuxGPUContext::cleanup()
{
    if(g_glxInitialized) {
        if(g_sharedContext)
            glXDestroyContext(g_x11Display, g_sharedContext);
        if(g_pbuffer)
            glXDestroyPbuffer(g_x11Display, g_pbuffer);
        g_sharedContext = nullptr;
        g_pbuffer = 0;
        g_mainContext = nullptr;
        g_drawable = 0;
        g_glxInitialized = false;
    }
    if(g_eglInitialized) {
        eglTerminate(g_eglDisplay);
        g_eglDisplay = EGL_NO_DISPLAY;
        g_eglInitialized = false;
    }
    if(g_x11Display) {
        XCloseDisplay(g_x11Display);
        g_x11Display = nullptr;
    }
    g_logger.info("LinuxGPUContext: resources cleaned up");
}

Display* LinuxGPUContext::x11Display() { return g_x11Display; }
GLXContext LinuxGPUContext::sharedContext() { return g_sharedContext; }
GLXContext LinuxGPUContext::mainContext() { return g_mainContext; }
GLXDrawable LinuxGPUContext::drawable() { return g_drawable; }
EGLDisplay LinuxGPUContext::eglDisplay() { return g_eglDisplay; }
bool LinuxGPUContext::eglSidecarReady() { return g_eglInitialized; }

#else
void LinuxGPUContext::initialize() {}
void LinuxGPUContext::cleanup() {}
#endif

