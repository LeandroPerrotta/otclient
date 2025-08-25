#pragma once

#include "../cef_renderer.h"
#include "gpuhelper.h"

class CefRendererGPULinuxNonMesa : public CefRenderer
{
public:
    explicit CefRendererGPULinuxNonMesa(UICEFWebView& view);
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    bool isSupported() const override;

private:
    int m_lastWidth;
    int m_lastHeight;
};
