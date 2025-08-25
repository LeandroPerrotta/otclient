#pragma once

#include "../cef_renderer.h"
#include "linux_gpu_context.h"
#include "gpu_helper.h"
#include <framework/graphics/texture.h>
#include <vector>

class CefRendererGPULinux : public CefRenderer
{
public:
    explicit CefRendererGPULinux(UICEFWebView& view);
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    void draw(Fw::DrawPane drawPane) override;
    bool isSupported() const override;

private:
    TexturePtr m_cefTexture;
    bool m_textureCreated;
    int m_lastWidth;
    int m_lastHeight;
};

