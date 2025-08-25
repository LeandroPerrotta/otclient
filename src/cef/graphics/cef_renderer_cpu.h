#pragma once

#include "cef_renderer.h"
#include <framework/graphics/texture.h>
#include <framework/graphics/image.h>

class CefRendererCPU : public CefRenderer
{
public:
    explicit CefRendererCPU(UICEFWebView& view);
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    void draw(Fw::DrawPane drawPane) override;
    bool isSupported() const override { return true; }

private:
    TexturePtr m_cefTexture;
    ImagePtr m_cefImage;
    bool m_textureCreated;
    int m_lastWidth;
    int m_lastHeight;
};
