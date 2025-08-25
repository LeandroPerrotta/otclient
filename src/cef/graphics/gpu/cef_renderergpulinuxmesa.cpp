#include "cef_renderergpulinuxmesa.h"
#include "linuxgpucontext.h"
#include "../../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <cstring>

static void* resolveGLProc(const char* name)
{
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
}

CefRendererGPULinuxMesa::CefRendererGPULinuxMesa(UICEFWebView& view)
    : CefRenderer(view)
    , m_lastWidth(0)
    , m_lastHeight(0)
    , m_checkedSupport(false)
    , m_supported(false)
    , m_glCreateMemoryObjectsEXT(nullptr)
    , m_glImportMemoryFdEXT(nullptr)
    , m_glTexStorageMem2DEXT(nullptr)
    , m_glDeleteMemoryObjectsEXT(nullptr)
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
    const int fd = info.planes[0].fd;
    if(fd < 0)
        return;
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    const int stride = info.planes[0].stride;
    const int offset = info.planes[0].offset;

    int memFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if(memFd < 0)
        return;

    g_dispatcher.addEventFromOtherThread([this, memFd, width, height, stride, offset]() mutable {
        auto close_fd = [](int& x){ if(x>=0){ ::close(x); x=-1; } };
        Display* x11Display = LinuxGPUContext::x11Display();
        if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
                close_fd(memFd); return; }
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
        GLuint memoryObject = 0;
        m_glCreateMemoryObjectsEXT(1, &memoryObject);
        if(glGetError() == GL_NO_ERROR && memoryObject != 0) {
            GLuint64 size = (GLuint64)height * stride;
            m_glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, memFd);
            if(glGetError() == GL_NO_ERROR) {
                m_glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, memoryObject, offset);
                if(glGetError() == GL_NO_ERROR) {
                    done = true;
                } else {
                    g_logger.error("CefRendererGPULinuxMesa: glTexStorageMem2DEXT failed");
                }
                memFd = -1;
            } else {
                g_logger.error("CefRendererGPULinuxMesa: glImportMemoryFdEXT failed");
            }
            m_glDeleteMemoryObjectsEXT(1, &memoryObject);
        } else {
            g_logger.error("CefRendererGPULinuxMesa: glCreateMemoryObjectsEXT failed");
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

bool CefRendererGPULinuxMesa::isSupported() const
{
#if defined(USE_CEF) && defined(__linux__)
    if(m_checkedSupport)
        return m_supported;
    m_checkedSupport = true;

    Display* x11Display = LinuxGPUContext::x11Display();
    if(!x11Display)
        return m_supported = false;

    if(glXGetCurrentContext() != LinuxGPUContext::mainContext())
        glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext());

    if(!isMesaDriver())
        return m_supported = false;

    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    if(!exts || !strstr(exts, "GL_EXT_memory_object_fd"))
        return m_supported = false;

    m_glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)resolveGLProc("glCreateMemoryObjectsEXT");
    m_glImportMemoryFdEXT = (PFNGLIMPORTMEMORYFDEXTPROC)resolveGLProc("glImportMemoryFdEXT");
    m_glTexStorageMem2DEXT = (PFNGLTEXSTORAGEMEM2DEXTPROC)resolveGLProc("glTexStorageMem2DEXT");
    m_glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)resolveGLProc("glDeleteMemoryObjectsEXT");

    if(!m_glCreateMemoryObjectsEXT || !m_glImportMemoryFdEXT ||
       !m_glTexStorageMem2DEXT || !m_glDeleteMemoryObjectsEXT)
        return m_supported = false;

    return m_supported = true;
#else
    return false;
#endif
}
