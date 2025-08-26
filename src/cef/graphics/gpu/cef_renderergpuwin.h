#pragma once

#include "../cef_renderer.h"
#include "gpuhelper.h"

#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

class CefRendererGPUWin : public CefRenderer
{
public:
    explicit CefRendererGPUWin(UICEFWebView& view);
    ~CefRendererGPUWin();
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    bool isSupported() const override;
    void onRenderSupported(CefWindowInfo& windowInfo) const override;

private:
    int m_lastWidth;
    int m_lastHeight;
    
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    // ANGLE D3D11 device (obtained from EGL)
    ID3D11Device* m_angleD3D11Device;
    ID3D11Device1* m_angleD3D11Device1;
    ID3D11DeviceContext* m_angleD3D11Context;
    
    // Our destination texture (created with classic shared handle)
    ID3D11Texture2D* m_angleDestTexture;
    HANDLE m_classicSharedHandle;
    
    // EGL pbuffer for texture sharing
    EGLSurface m_pbuffer;
    EGLConfig m_eglConfig;
    bool m_pbufferBound;
    
    // Extension function pointers
    PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
    PFNEGLQUERYDEVICEATTRIBEXTPROC eglQueryDeviceAttribEXT;
    
    // Initialization and cleanup
    bool initializeAngleInterop();
    void cleanupResources();
    bool createAngleDestinationTexture(int width, int height);
    bool setupEGLPbuffer();
    void cleanupEGLPbuffer();
    
    // Per-frame operations
    bool copyFromCEFTexture(HANDLE ntHandle);
    bool openSharedResourceSafely(HANDLE handle, ID3D11Texture2D** outTexture);
    bool handleKeyedMutex(ID3D11Texture2D* srcTexture, ID3D11Texture2D* dstTexture, bool acquire);
#endif
};

