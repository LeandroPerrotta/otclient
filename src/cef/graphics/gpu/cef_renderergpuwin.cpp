#include "cef_renderergpuwin.h"
#include "../../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/graphics/graphics.h>
#include "../../core/cef_init.h"
#if defined(USE_CEF)
#include <include/cef_browser.h>
#endif
#if defined(USE_CEF) && defined(_WIN32)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

CefRendererGPUWin::CefRendererGPUWin(UICEFWebView& view)
    : CefRenderer(view)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
}

void CefRendererGPUWin::onPaint(const void* buffer, int width, int height,
                                const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPUWin::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    HANDLE sharedHandle = static_cast<HANDLE>(info.shared_texture_handle);
    if (!sharedHandle || sharedHandle == INVALID_HANDLE_VALUE) {
        g_logger.error("CefRendererGPUWin: Invalid shared texture handle");
        return;
    }

    g_logger.debug(stdext::format("CefRendererGPUWin: Processing shared handle %p, size: %dx%d", 
                                  sharedHandle, info.extra.coded_size.width, info.extra.coded_size.height));

    // Create or update OpenGL texture if needed
    if (!m_cefTexture || m_lastWidth != info.extra.coded_size.width || m_lastHeight != info.extra.coded_size.height) {
        m_cefTexture = TexturePtr(new Texture(Size(info.extra.coded_size.width, info.extra.coded_size.height)));
        m_lastWidth = info.extra.coded_size.width;
        m_lastHeight = info.extra.coded_size.height;
        m_textureCreated = true;
        g_logger.info(stdext::format("CefRendererGPUWin: Created new texture %dx%d", m_lastWidth, m_lastHeight));
    }

    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        g_logger.error("CefRendererGPUWin: No current EGL display");
        return;
    }

    // Choose EGL config for pbuffer creation
    EGLConfig config;
    EGLint numCfg;
    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    if (!eglChooseConfig(display, cfgAttrs, &config, 1, &numCfg) || numCfg == 0) {
        g_logger.error("CefRendererGPUWin: Failed to choose EGL config");
        return;
    }

    // Create pbuffer attributes
    EGLint attrs[] = {
        EGL_WIDTH, m_lastWidth,
        EGL_HEIGHT, m_lastHeight,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    // Create pbuffer from the shared D3D handle
    EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(
        display,
        EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        (EGLClientBuffer)sharedHandle,
        config,
        attrs
    );

    if (pbuffer == EGL_NO_SURFACE) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("CefRendererGPUWin: eglCreatePbufferFromClientBuffer failed - EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        
        // Additional diagnostics
        if (eglError == EGL_BAD_PARAMETER) {
            g_logger.error("CefRendererGPUWin: EGL_BAD_PARAMETER - Check if handle is valid and ANGLE supports D3D interop");
        } else if (eglError == EGL_BAD_ALLOC) {
            g_logger.error("CefRendererGPUWin: EGL_BAD_ALLOC - Out of memory");
        }
        return;
    }

    // Bind the pbuffer to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
    
    if (!eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER)) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("CefRendererGPUWin: eglBindTexImage failed - EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        eglDestroySurface(display, pbuffer);
        return;
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Cleanup
    eglReleaseTexImage(display, pbuffer, EGL_BACK_BUFFER);
    eglDestroySurface(display, pbuffer);

    g_logger.debug("CefRendererGPUWin: Successfully bound shared texture to OpenGL");
#else
    (void)info;
#endif
}

bool CefRendererGPUWin::isSupported() const
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    // Check if ANGLE D3D texture sharing extension is available
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    return extensions && strstr(extensions, "EGL_ANGLE_d3d_share_handle_client_buffer") != nullptr;
#else
    return false;
#endif
}

void CefRendererGPUWin::onRenderSupported(CefWindowInfo& windowInfo) const
{
    if (g_cefConfig && g_cefConfig->shouldUseSharedTexture() && isSupported()) {
        windowInfo.shared_texture_enabled = true;
        g_logger.info("CefRendererGPUWin: Shared texture enabled for CEF browser");
    }
}

