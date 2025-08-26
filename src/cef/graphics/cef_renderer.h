#pragma once

#include <framework/graphics/graphics.h>

#ifdef USE_CEF
#include "include/cef_render_handler.h"
struct CefWindowInfo;
#endif

class UICEFWebView;

class CefRenderer
{
public:
    explicit CefRenderer(UICEFWebView& view) : m_view(view) {}
    virtual ~CefRenderer() = default;

#ifdef USE_CEF
    virtual void onPaint(const void* buffer, int width, int height,
                         const CefRenderHandler::RectList& dirtyRects) = 0;
    virtual void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) = 0;
    virtual void onRenderSupported(CefWindowInfo& windowInfo) const;
#endif
    virtual void draw(Fw::DrawPane drawPane);
    virtual bool isSupported() const { return true; }

protected:
    UICEFWebView& m_view;
    TexturePtr m_cefTexture;
    bool m_textureCreated = false;
};
