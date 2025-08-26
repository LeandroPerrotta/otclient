#pragma once

#include "../cef_renderer.h"
#include "gpuhelper.h"

#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
#include <d3d11.h>
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
    // D3D11 resources for shared texture handling
    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dContext;
    
    // EGL extensions and capabilities
    bool m_angleExtensionsChecked;
    bool m_hasD3D11Extension;
    bool m_hasImageBaseExtension;
    
    // EGL function pointers for ANGLE extensions
    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    
    // Initialization and cleanup methods
    bool initializeD3DDevice();
    void cleanupD3DResources();
    bool checkAngleExtensions();
    bool createTextureFromSharedHandle(HANDLE sharedHandle, const CefAcceleratedPaintInfo& info);
#endif
};

