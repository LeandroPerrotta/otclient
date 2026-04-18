#pragma once

#include "../cef_renderer.h"
#include "gpuhelper.h"

class CefRendererGPULinuxNonMesa : public CefRenderer
{
public:
    explicit CefRendererGPULinuxNonMesa(UICEFWebView& view);
    void draw(Fw::DrawPane drawPane) override;
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info, const CefRenderHandler::RectList* dirtyRects = nullptr) override;
    bool isSupported() const override;
    void onRenderSupported(CefWindowInfo& windowInfo) const override;

private:
};
