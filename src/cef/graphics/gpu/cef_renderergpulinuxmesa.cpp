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

void CefRendererGPULinuxMesa::onAcceleratedPaint(const CefAcceleratedPaintInfo& info, const CefRenderHandler::RectList* dirtyRects)
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
        
        // Save current context
        GLXContext currentContext = glXGetCurrentContext();
        GLXDrawable currentDrawable = glXGetCurrentDrawable();
        
        // Ensure we're using the correct context for CEF operations
        if(currentContext != LinuxGPUContext::mainContext()) {
            if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
                g_logger.error("CefRendererGPULinuxMesa: Failed to make main context current");
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
        
        // Clear any existing GL errors before proceeding
        while(glGetError() != GL_NO_ERROR) { /* clear errors */ }
        
        m_glCreateMemoryObjectsEXT(1, &memoryObject);
        GLenum err = glGetError();
        if(err == GL_NO_ERROR && memoryObject != 0) {
            GLuint64 size = (GLuint64)height * stride;
            m_glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, memFd);
            err = glGetError();
            if(err == GL_NO_ERROR) {
                m_glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, memoryObject, offset);
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
            m_glDeleteMemoryObjectsEXT(1, &memoryObject);
        } else {
            g_logger.error(stdext::format("CefRendererGPULinuxMesa: glCreateMemoryObjectsEXT failed with error 0x%x", err));
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Restore original context if it was different
        if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
            if(!glXMakeCurrent(x11Display, currentDrawable, currentContext)) {
                g_logger.warning("CefRendererGPULinuxMesa: Failed to restore original context");
            }
        }
        
        close_fd(memFd);
        if(!done) {
            g_logger.error("CefRendererGPULinuxMesa: GPU import failed");
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

    // Save current context for restoration
    GLXContext currentContext = glXGetCurrentContext();
    GLXDrawable currentDrawable = glXGetCurrentDrawable();
    
    // Make sure we have the main context active for extension checks
    if(currentContext != LinuxGPUContext::mainContext()) {
        if(!glXMakeCurrent(x11Display, LinuxGPUContext::drawable(), LinuxGPUContext::mainContext())) {
            g_logger.info("CefRendererGPULinuxMesa: Failed to make main context current");
            return m_supported = false;
        }
    }

    if(!isMesaDriver()) {
        g_logger.info("CefRendererGPULinuxMesa: Not a Mesa driver");
        // Restore context before returning
        if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
            glXMakeCurrent(x11Display, currentDrawable, currentContext);
        }
        return m_supported = false;
    }

    const char* exts = (const char*)glGetString(GL_EXTENSIONS);
    if(!exts || !strstr(exts, "GL_EXT_memory_object_fd")){
        g_logger.info("CefRendererGPULinuxMesa: GL_EXT_memory_object_fd not supported");
        // Restore context before returning
        if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
            glXMakeCurrent(x11Display, currentDrawable, currentContext);
        }
        return m_supported = false;
    }
        
    m_glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)resolveGLProc("glCreateMemoryObjectsEXT");
    m_glImportMemoryFdEXT = (PFNGLIMPORTMEMORYFDEXTPROC)resolveGLProc("glImportMemoryFdEXT");
    m_glTexStorageMem2DEXT = (PFNGLTEXSTORAGEMEM2DEXTPROC)resolveGLProc("glTexStorageMem2DEXT");
    m_glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)resolveGLProc("glDeleteMemoryObjectsEXT");

    if(!m_glCreateMemoryObjectsEXT || !m_glImportMemoryFdEXT ||
       !m_glTexStorageMem2DEXT || !m_glDeleteMemoryObjectsEXT) {
        g_logger.info("CefRendererGPULinuxMesa: GL_EXT_memory_object_fd function pointers not available");
        // Restore context before returning
        if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
            glXMakeCurrent(x11Display, currentDrawable, currentContext);
        }
        return m_supported = false;
    }
    
    // Restore original context
    if(currentContext != LinuxGPUContext::mainContext() && currentContext != nullptr) {
        if(!glXMakeCurrent(x11Display, currentDrawable, currentContext)) {
            g_logger.warning("CefRendererGPULinuxMesa: Failed to restore original context during support check");
        }
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