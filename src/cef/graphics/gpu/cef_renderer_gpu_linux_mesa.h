#pragma once

#include "../cef_renderer.h"
#include "gpu_helper.h"
#include <framework/graphics/texture.h>

class CefRendererGPULinuxMesa : public CefRenderer
{
public:
    explicit CefRendererGPULinuxMesa(UICEFWebView& view);
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
