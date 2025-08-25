#include "cef_renderer_gpu_linux.h"
#include "linux_gpu_context.h"
#include "gpu_helper.h"
#include "../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <GL/glx.h>
#include <drm/drm_fourcc.h>
#include <GL/gl.h>
#include <thread>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC g_glEGLImageTargetTexture2DOES = nullptr;

static void* resolveGLProc(const char* name)
{
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
}

static bool ensureGlEglImageProcResolved()
{
    if(g_glEGLImageTargetTexture2DOES)
        return true;
    g_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)resolveGLProc("glEGLImageTargetTexture2DOES");
    return g_glEGLImageTargetTexture2DOES != nullptr;
}

CefRendererGPULinux::CefRendererGPULinux(UICEFWebView& view)
    : CefRenderer(view)
    , m_cefTexture(nullptr)
    , m_textureCreated(false)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
    LinuxGPUContext::initialize();
}

void CefRendererGPULinux::onPaint(const void* buffer, int width, int height,
                                  const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPULinux::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(USE_CEF) && defined(__linux__)
    if(!LinuxGPUContext::eglSidecarReady()) {
        g_logger.error("CefRendererGPULinux: EGL sidecar not initialized");
        return;
    }
    const int fd = info.planes[0].fd;
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    const int stride = info.planes[0].stride;
    const int offset = info.planes[0].offset;
    if(fd < 0)
        return;

    int dupFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if(dupFd < 0)
        return;

    g_dispatcher.addEventFromOtherThread([this, dupFd, width, height, stride, offset]() mutable {
        auto close_fd = [](int& x){ if(x>=0){ ::close(x); x=-1;} };
        if(!ensureGlEglImageProcResolved()) { close_fd(dupFd); return; }
        Display* x11Display = LinuxGPUContext::x11Display();
        if(!x11Display) { close_fd(dupFd); return; }
        if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
                close_fd(dupFd); return; }
        }
        if(!m_cefTexture || width != m_lastWidth || height != m_lastHeight) {
            m_cefTexture = TexturePtr(new Texture(Size(width, height)));
            m_lastWidth = width;
            m_lastHeight = height;
            m_textureCreated = true;
        }
        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

        EGLint attrs[] = {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
            EGL_DMA_BUF_PLANE0_FD_EXT, dupFd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
            EGL_NONE
        };
        EGLImageKHR image = eglCreateImageKHR(LinuxGPUContext::eglDisplay(), EGL_NO_CONTEXT,
                                             EGL_LINUX_DMA_BUF_EXT, nullptr, attrs);
        if(image != EGL_NO_IMAGE_KHR) {
            g_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
            eglDestroyImageKHR(LinuxGPUContext::eglDisplay(), image);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        close_fd(dupFd);
    });
#else
    (void)info;
#endif
}

void CefRendererGPULinux::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    } else {
        m_view.UIWidget::drawSelf(drawPane);
    }
}

bool CefRendererGPULinux::isSupported() const
{
#if defined(USE_CEF) && defined(__linux__)
    return LinuxGPUContext::eglSidecarReady();
#else
    return false;
#endif
}

