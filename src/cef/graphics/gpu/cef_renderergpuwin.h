#pragma once

#include "../cef_renderer.h"
#include "gpuhelper.h"

class CefRendererGPUWin : public CefRenderer
{
public:
    explicit CefRendererGPUWin(UICEFWebView& view);
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    bool isSupported() const override;

private:
    int m_lastWidth;
    int m_lastHeight;
};

