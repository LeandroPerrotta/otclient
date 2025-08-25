#include "cef_renderer_gpu_win.h"
#include "../../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/graphics/graphics.h>
#if defined(USE_CEF) && defined(_WIN32)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

CefRendererGPUWin::CefRendererGPUWin(UICEFWebView& view)
    : CefRenderer(view)
    , m_cefTexture(nullptr)
    , m_textureCreated(false)
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
    if(!sharedHandle) {
        g_logger.error("CefRendererGPUWin: No shared texture handle");
        return;
    }
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    D3D_FEATURE_LEVEL fl;
    if(FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, &fl, &d3dContext))) {
        g_logger.error("CefRendererGPUWin: Failed to create D3D11 device");
        return;
    }
    ID3D11Texture2D* tex = nullptr;
    if(FAILED(d3dDevice->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)&tex))) {
        g_logger.error("CefRendererGPUWin: OpenSharedResource failed");
        d3dDevice->Release();
        d3dContext->Release();
        return;
    }
    if(!m_cefTexture || m_lastWidth != info.extra.coded_size.width || m_lastHeight != info.extra.coded_size.height) {
        m_cefTexture = TexturePtr(new Texture(Size(info.extra.coded_size.width, info.extra.coded_size.height)));
        m_lastWidth = info.extra.coded_size.width;
        m_lastHeight = info.extra.coded_size.height;
        m_textureCreated = true;
    }
    EGLDisplay display = eglGetCurrentDisplay();
    EGLConfig config;
    EGLint numCfg;
    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
        EGL_NONE
    };
    eglChooseConfig(display, cfgAttrs, &config, 1, &numCfg);
    EGLint attrs[] = {
        EGL_WIDTH, m_lastWidth,
        EGL_HEIGHT, m_lastHeight,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };
    EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(display,
        EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        (EGLClientBuffer)sharedHandle,
        config,
        attrs);
    if(pbuffer == EGL_NO_SURFACE) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("eglCreatePbufferFromClientBuffer failed - EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
    } else {
        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
        eglReleaseTexImage(display, pbuffer, EGL_BACK_BUFFER);
        eglDestroySurface(display, pbuffer);
    }
    tex->Release();
    d3dDevice->Release();
    d3dContext->Release();
#else
    (void)info;
#endif
}

void CefRendererGPUWin::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    }
}

bool CefRendererGPUWin::isSupported() const
{
#if defined(USE_CEF) && defined(_WIN32)
    return true;
#else
    return false;
#endif
}

