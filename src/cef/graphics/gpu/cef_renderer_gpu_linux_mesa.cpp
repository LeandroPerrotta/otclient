#include "cef_renderer_gpu_linux_mesa.h"
#include "linux_gpu_context.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <cstring>

#ifndef GL_EXT_memory_object_fd
#define GL_EXT_memory_object_fd 1
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
typedef void (APIENTRYP PFNGLCREATEMEMORYOBJECTSEXTPROC)(GLsizei n, GLuint* memoryObjects);
typedef void (APIENTRYP PFNGLIMPORTMEMORYFDEXTPROC)(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM2DEXTPROC)(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLDELETEMEMORYOBJECTSEXTPROC)(GLsizei n, const GLuint* memoryObjects);
#endif

static void* resolveGLProc(const char* name)
{
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
}

CefRendererGPULinuxMesa::CefRendererGPULinuxMesa(UICEFWebView& view)
    : CefRenderer(view)
    , m_cefTexture(nullptr)
    , m_textureCreated(false)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
    LinuxGPUContext::initialize();
}

void CefRendererGPULinuxMesa::onPaint(const void* buffer, int width, int height,
                                      const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPULinuxMesa::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(USE_CEF) && defined(__linux__)
    if(!LinuxGPUContext::eglSidecarReady()) {
        g_logger.error("CefRendererGPULinuxMesa: EGL sidecar not initialized");
        return;
    }
    const int fd = info.planes[0].fd;
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    const int stride = info.planes[0].stride;
    const int offset = info.planes[0].offset;
    if(fd < 0)
        return;

    int memFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if(memFd < 0)
        return;

    g_dispatcher.addEventFromOtherThread([this, memFd, width, height, stride, offset]() mutable {
        auto close_fd = [](int& x){ if(x>=0){ ::close(x); x=-1; } };
        Display* x11Display = LinuxGPUContext::x11Display();
        if(!x11Display) { close_fd(memFd); return; }
        if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
                close_fd(memFd); return; }
        }

        const char* exts = (const char*)glGetString(GL_EXTENSIONS);
        if(!exts || !strstr(exts, "GL_EXT_memory_object_fd")) { close_fd(memFd); return; }

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
        auto glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)resolveGLProc("glCreateMemoryObjectsEXT");
        auto glImportMemoryFdEXT = (PFNGLIMPORTMEMORYFDEXTPROC)resolveGLProc("glImportMemoryFdEXT");
        auto glTexStorageMem2DEXT = (PFNGLTEXSTORAGEMEM2DEXTPROC)resolveGLProc("glTexStorageMem2DEXT");
        auto glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)resolveGLProc("glDeleteMemoryObjectsEXT");
        if(glCreateMemoryObjectsEXT && glImportMemoryFdEXT && glTexStorageMem2DEXT && glDeleteMemoryObjectsEXT) {
            GLuint memoryObject = 0;
            glCreateMemoryObjectsEXT(1, &memoryObject);
            if(glGetError() == GL_NO_ERROR && memoryObject != 0) {
                GLuint64 size = (GLuint64)height * stride;
                glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, memFd);
                if(glGetError() == GL_NO_ERROR) {
                    glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, memoryObject, offset);
                    if(glGetError() == GL_NO_ERROR) {
                        done = true;
                    } else {
                        g_logger.error("CefRendererGPULinuxMesa: glTexStorageMem2DEXT failed");
                    }
                    memFd = -1;
                } else {
                    g_logger.error("CefRendererGPULinuxMesa: glImportMemoryFdEXT failed");
                }
                glDeleteMemoryObjectsEXT(1, &memoryObject);
            } else {
                g_logger.error("CefRendererGPULinuxMesa: glCreateMemoryObjectsEXT failed");
            }
        } else {
            g_logger.error("CefRendererGPULinuxMesa: memory object functions missing");
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        close_fd(memFd);
        if(!done) {
            g_logger.error("CefRendererGPULinuxMesa: GPU import failed");
        }
    });
#else
    (void)info;
#endif
}

void CefRendererGPULinuxMesa::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    }
}

bool CefRendererGPULinuxMesa::isSupported() const
{
#if defined(USE_CEF) && defined(__linux__)
    if(!LinuxGPUContext::eglSidecarReady())
        return false;
    Display* x11Display = LinuxGPUContext::x11Display();
    if(!x11Display)
        return false;
    if(glXGetCurrentContext() != LinuxGPUContext::mainContext())
        glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext());
    if(!isMesaDriver())
        return false;
    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    return exts && strstr(exts, "GL_EXT_memory_object_fd");
#else
    return false;
#endif
}
