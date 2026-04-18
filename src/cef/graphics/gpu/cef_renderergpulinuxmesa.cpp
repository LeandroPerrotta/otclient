#include "cef_renderergpulinuxmesa.h"
#include "linuxgpucontext.h"
#include "../../ui/uicefwebview.h"
#include "../../core/cef_init.h"
#include "../../core/cef_config.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <atomic>
#include <memory>

static void* resolveGLProc(const char* name)
{
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
}

CefRendererGPULinuxMesa::CefRendererGPULinuxMesa(UICEFWebView& view)
    : CefRenderer(view)
    , m_checkedSupport(false)
    , m_supported(false)
    , m_glCreateMemoryObjectsEXT(nullptr)
    , m_glImportMemoryFdEXT(nullptr)
    , m_glTexStorageMem2DEXT(nullptr)
    , m_glDeleteMemoryObjectsEXT(nullptr)
{
    LinuxGPUContext::initialize();
}

void CefRendererGPULinuxMesa::draw(Fw::DrawPane drawPane)
{
    auto ch = m_view.getGpuPaintChannel();
    if(ch && ch->textureReady.load() && ch->texture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0f);
        g_painter->drawTexturedRect(rect, ch->texture);
    }
    (void)drawPane;
}

void CefRendererGPULinuxMesa::onPaint(const void* buffer, int width, int height,
                                      const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPULinuxMesa::onAcceleratedPaint(const CefAcceleratedPaintInfo& info, const CefRenderHandler::RectList* dirtyRects)
{
#if defined(USE_CEF) && defined(__linux__)
    if(!isSupported()) {
        return;
    }
    if(!m_glCreateMemoryObjectsEXT || !m_glImportMemoryFdEXT || !m_glTexStorageMem2DEXT || !m_glDeleteMemoryObjectsEXT) {
        return;
    }

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

    std::shared_ptr<CefGpuPaintChannel> ch = m_view.getGpuPaintChannel();
    if(!ch) {
        ::close(memFd);
        return;
    }

    PFNGLCREATEMEMORYOBJECTSEXTPROC fnCreate = m_glCreateMemoryObjectsEXT;
    PFNGLIMPORTMEMORYFDEXTPROC fnImport = m_glImportMemoryFdEXT;
    PFNGLTEXSTORAGEMEM2DEXTPROC fnTexStorage = m_glTexStorageMem2DEXT;
    PFNGLDELETEMEMORYOBJECTSEXTPROC fnDeleteMem = m_glDeleteMemoryObjectsEXT;

    g_dispatcher.addEventFromOtherThread([ch, memFd, width, height, stride, offset, fnCreate, fnImport, fnTexStorage, fnDeleteMem]() mutable {
        auto close_fd = [](int& x){ if(x>=0){ ::close(x); x=-1; } };
        if(!ch->alive.load()) {
            close_fd(memFd);
            return;
        }
        if(!fnCreate || !fnImport || !fnTexStorage || !fnDeleteMem) {
            close_fd(memFd);
            return;
        }
        if(!LinuxGPUContext::glxReady()) {
            close_fd(memFd);
            return;
        }
        Display* x11Display = LinuxGPUContext::x11Display();

        if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
                g_logger.error("CefRendererGPULinuxMesa: Failed to make main context current");
                close_fd(memFd);
                return;
            }
        }

        if(!ch->alive.load()) {
            close_fd(memFd);
            return;
        }

        ch->textureReady.store(false);
        ch->texture = TexturePtr(new Texture(Size(width, height)));

        glBindTexture(GL_TEXTURE_2D, ch->texture->getId());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

        // Drain any pending GL errors left by the game's rendering pipeline
        // (lightview FBOs, shaders, etc.) before our own GL calls, otherwise
        // glGetError() will report a stale error as if our call failed.
        while(glGetError() != GL_NO_ERROR) {}

        if(!ch->alive.load()) {
            ch->texture.reset();
            close_fd(memFd);
            return;
        }

        bool done = false;
        GLuint memoryObject = 0;
        fnCreate(1, &memoryObject);
        if(memoryObject != 0) {
            GLuint64 size = (GLuint64)height * stride;
            fnImport(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, memFd);
            GLenum err = glGetError();
            if(err == GL_NO_ERROR) {
                fnTexStorage(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, memoryObject, offset);
                err = glGetError();
                if(err == GL_NO_ERROR) {
                    done = true;
                } else {
                    g_logger.error(stdext::format("CefRendererGPULinuxMesa: glTexStorageMem2DEXT failed with error 0x%x", err));
                }
                memFd = -1;
            } else {
                g_logger.error(stdext::format("CefRendererGPULinuxMesa: glImportMemoryFdEXT failed with error 0x%x", err));
            }
            fnDeleteMem(1, &memoryObject);
        } else {
            g_logger.error("CefRendererGPULinuxMesa: glCreateMemoryObjectsEXT failed (returned 0)");
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        // Upstream saved/restored the previous (context, drawable). On otclient-retro that restore
        // could leave GLX in a state where the next UI frame renders black. OTClient's contract is
        // that the main-thread GL user is always the game's window context — re-bind it explicitly.
        if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
            g_logger.warning("CefRendererGPULinuxMesa: Failed to re-bind main context after accelerated paint");
        }

        close_fd(memFd);
        if(!done) {
            ch->texture.reset();
            ch->textureReady.store(false);
            g_logger.error("CefRendererGPULinuxMesa: GPU import failed");
        } else if(ch->alive.load()) {
            ch->textureReady.store(true);
        } else {
            ch->texture.reset();
            ch->textureReady.store(false);
        }
    });
  #else
    (void)info;
    (void)dirtyRects;
  #endif
}

bool CefRendererGPULinuxMesa::isSupported() const
{
#if defined(USE_CEF) && defined(__linux__)
    if(m_checkedSupport)
        return m_supported;
    m_checkedSupport = true;

    if(g_cefConfig && !g_cefConfig->shouldUseSharedTexture()) {
        g_logger.info("CefRendererGPULinuxMesa: Shared texture disabled by config");
        return m_supported = false;
    }    

    Display* x11Display = LinuxGPUContext::x11Display();
    if(!x11Display) {
        g_logger.info("CefRendererGPULinuxMesa: No X11 display");
        return m_supported = false;
    }

    if(glXGetCurrentContext() != LinuxGPUContext::mainContext()) {
        if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
            g_logger.info("CefRendererGPULinuxMesa: Failed to make main context current");
            return m_supported = false;
        }
    }

    if(!isMesaDriver()) {
        g_logger.info("CefRendererGPULinuxMesa: Not a Mesa driver");
        return m_supported = false;
    }

    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    if(!exts || !strstr(exts, "GL_EXT_memory_object_fd")){
        g_logger.info("CefRendererGPULinuxMesa: GL_EXT_memory_object_fd not supported");
        return m_supported = false;
    }
        

    m_glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)resolveGLProc("glCreateMemoryObjectsEXT");
    m_glImportMemoryFdEXT = (PFNGLIMPORTMEMORYFDEXTPROC)resolveGLProc("glImportMemoryFdEXT");
    m_glTexStorageMem2DEXT = (PFNGLTEXSTORAGEMEM2DEXTPROC)resolveGLProc("glTexStorageMem2DEXT");
    m_glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)resolveGLProc("glDeleteMemoryObjectsEXT");

    if(!m_glCreateMemoryObjectsEXT || !m_glImportMemoryFdEXT ||
       !m_glTexStorageMem2DEXT || !m_glDeleteMemoryObjectsEXT) {
        g_logger.info("CefRendererGPULinuxMesa: GL_EXT_memory_object_fd function pointers not available");
        return m_supported = false;
    }

    g_logger.info("CefRendererGPULinuxMesa: Supported");
    return m_supported = true;
#else
    return false;
#endif
}

void CefRendererGPULinuxMesa::onRenderSupported(CefWindowInfo& windowInfo) const
{
    if (isSupported()) {
        windowInfo.shared_texture_enabled = true;
        g_logger.info("CefRendererGPULinuxMesa: Shared texture enabled for CEF browser");
    }
}