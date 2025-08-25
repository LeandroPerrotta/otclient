#include "cef_renderer_gpu_linux_nonmesa.h"
#include "linux_gpu_context.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <framework/stdext.h>
#include <GL/glx.h>
#include <drm/drm_fourcc.h>
#include <GL/gl.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstring>

#ifndef GLeglImageOES
typedef void* GLeglImageOES;
#endif
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
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

static bool isDmaBufModifierSupported(EGLDisplay display, EGLint format, uint64_t modifier)
{
#if defined(__linux__)
    auto queryFormats = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    auto queryModifiers = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    if(!queryFormats || !queryModifiers)
        return false;

    EGLint numFormats = 0;
    if(!queryFormats(display, 0, nullptr, &numFormats) || numFormats <= 0)
        return false;

    std::vector<EGLint> formats(numFormats);
    if(!queryFormats(display, numFormats, formats.data(), &numFormats))
        return false;

    if(std::find(formats.begin(), formats.end(), format) == formats.end())
        return false;

    EGLint numModifiers = 0;
    if(!queryModifiers(display, format, 0, nullptr, nullptr, &numModifiers) || numModifiers <= 0)
        return false;

    std::vector<EGLuint64KHR> modifiers(numModifiers);
    std::vector<EGLBoolean> externalOnly(numModifiers);
    if(!queryModifiers(display, format, numModifiers, modifiers.data(), externalOnly.data(), &numModifiers))
        return false;

    return std::find(modifiers.begin(), modifiers.end(), modifier) != modifiers.end();
#else
    (void)display; (void)format; (void)modifier; return false;
#endif
}

CefRendererGPULinuxNonMesa::CefRendererGPULinuxNonMesa(UICEFWebView& view)
    : CefRenderer(view)
    , m_cefTexture(nullptr)
    , m_textureCreated(false)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
    LinuxGPUContext::initialize();
}

void CefRendererGPULinuxNonMesa::onPaint(const void* buffer, int width, int height,
                                         const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPULinuxNonMesa::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(USE_CEF) && defined(__linux__)
    if(!LinuxGPUContext::eglSidecarReady()) {
        g_logger.error("CefRendererGPULinuxNonMesa: EGL sidecar not initialized");
        return;
    }
    const int fd = info.planes[0].fd;
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    const int stride = info.planes[0].stride;
    const int offset = info.planes[0].offset;
    const uint64_t modifier = info.modifier;
    if(fd < 0)
        return;

    int eglFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if(eglFd < 0)
        return;

    g_dispatcher.addEventFromOtherThread([this, eglFd, width, height, stride, offset, modifier]() mutable {
        auto close_fd = [](int& x){ if(x>=0){ ::close(x); x=-1; } };
        if(!ensureGlEglImageProcResolved()) { close_fd(eglFd); return; }
        Display* x11Display = LinuxGPUContext::x11Display();
        if(!x11Display) { close_fd(eglFd); return; }
        if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) { close_fd(eglFd); return; }
        }

        auto eglCreateImageKHRFunc = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        auto eglDestroyImageKHRFunc = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if(!eglCreateImageKHRFunc || !eglDestroyImageKHRFunc) {
            g_logger.error("CefRendererGPULinuxNonMesa: eglCreateImageKHR/eglDestroyImageKHR not available");
            close_fd(eglFd); return;
        }

        m_cefTexture = TexturePtr(new Texture(Size(width, height)));
        m_textureCreated = true;
        m_lastWidth = width;
        m_lastHeight = height;

        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

        bool done = false;
        auto buildAttrs = [&](bool includeModifier) {
            std::vector<EGLint> a = {
                EGL_WIDTH, width,
                EGL_HEIGHT, height,
                EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
                EGL_DMA_BUF_PLANE0_FD_EXT, eglFd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, stride
            };
            if(includeModifier) {
                a.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT);
                a.push_back((EGLint)(modifier & 0xffffffffu));
                a.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT);
                a.push_back((EGLint)(modifier >> 32));
            }
            a.push_back(EGL_NONE);
            return a;
        };

        bool canUseMod = (modifier != DRM_FORMAT_MOD_INVALID && modifier != 0) &&
                         isDmaBufModifierSupported(LinuxGPUContext::eglDisplay(), DRM_FORMAT_ARGB8888, modifier);

        auto attrs = buildAttrs(canUseMod);
        EGLImageKHR image = eglCreateImageKHRFunc(LinuxGPUContext::eglDisplay(), EGL_NO_CONTEXT,
                                                 EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());

        if(image == EGL_NO_IMAGE_KHR && canUseMod) {
            EGLint err = eglGetError();
            g_logger.info(stdext::format("CefRendererGPULinuxNonMesa: eglCreateImage w/ modifier failed (0x%x), retrying w/o modifier", err));
            attrs = buildAttrs(false);
            image = eglCreateImageKHRFunc(LinuxGPUContext::eglDisplay(), EGL_NO_CONTEXT,
                                         EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
        }

        if(image != EGL_NO_IMAGE_KHR) {
            close_fd(eglFd);
            g_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
            GLenum glErr = glGetError();
            eglDestroyImageKHRFunc(LinuxGPUContext::eglDisplay(), image);
            if(glErr == GL_NO_ERROR) {
                done = true;
            } else {
                g_logger.error(stdext::format("CefRendererGPULinuxNonMesa: glEGLImageTargetTexture2DOES failed 0x%x", glErr));
            }
        } else {
            EGLint err = eglGetError();
            g_logger.error(stdext::format("CefRendererGPULinuxNonMesa: eglCreateImageKHR failed 0x%x", err));
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        close_fd(eglFd);
        if(!done) {
            g_logger.error("CefRendererGPULinuxNonMesa: GPU import failed");
        }
    });
#else
    (void)info;
#endif
}

void CefRendererGPULinuxNonMesa::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    }
}

bool CefRendererGPULinuxNonMesa::isSupported() const
{
#if defined(USE_CEF) && defined(__linux__)
    if(!LinuxGPUContext::eglSidecarReady())
        return false;
    Display* x11Display = LinuxGPUContext::x11Display();
    if(!x11Display)
        return false;
    if(glXGetCurrentContext() != LinuxGPUContext::mainContext())
        glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext());
    if(isMesaDriver())
        return false;
    if(!ensureGlEglImageProcResolved())
        return false;
    auto eglCreateImageKHRFunc = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    auto eglDestroyImageKHRFunc = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    return eglCreateImageKHRFunc && eglDestroyImageKHRFunc;
#else
    return false;
#endif
}
