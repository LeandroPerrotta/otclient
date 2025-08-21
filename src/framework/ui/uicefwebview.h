#pragma once

#include "uiwebview.h"

#ifdef USE_CEF
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include <framework/graphics/texture.h>
#include <framework/graphics/image.h>

// Forward declaration
class SimpleCEFClient;
#endif

class UICEFWebView : public UIWebView {
public:
    UICEFWebView();
    UICEFWebView(UIWidgetPtr parent);
    virtual ~UICEFWebView();

    // CEF-specific methods
    void onCEFPaint(const void* buffer, int width, int height, const CefRenderHandler::RectList& dirtyRects);
    void onCEFAcceleratedPaint(void* sharedHandle);
    void onBrowserCreated(CefRefPtr<CefBrowser> browser);
    
    // Static methods for managing all WebViews
    static void closeAllWebViews();
    static size_t getActiveWebViewCount();

    static void setAllWindowlessFrameRate(int fps);

    void setWindowlessFrameRate(int fps);

    // Event handlers
    void onLoadStarted() override;
    void onLoadFinished(bool success) override;
    void onUrlChanged(const std::string& url) override;
    void onTitleChanged(const std::string& title) override;
    void onJavaScriptCallback(const std::string& name, const std::string& data) override;

    // Mouse input handlers
    bool onMousePress(const Point& mousePos, Fw::MouseButton button) override;
    bool onMouseRelease(const Point& mousePos, Fw::MouseButton button) override;
    bool onMouseMove(const Point& mousePos, const Point& mouseMoved) override;
    bool onMouseWheel(const Point& mousePos, Fw::MouseWheelDirection direction) override;
    void onHoverChange(bool hovered) override;

    // Keyboard input handlers
    bool onKeyText(const std::string& keyText) override;
    bool onKeyDown(uchar keyCode, int keyboardModifiers) override;
    bool onKeyPress(uchar keyCode, int keyboardModifiers, int autoRepeatTicks) override;
    bool onKeyUp(uchar keyCode, int keyboardModifiers) override;    

    // Geometry change handler for dynamic resizing
    void onGeometryChange(const Rect& oldRect, const Rect& newRect) override;

protected:
    void createWebView() override;
    void loadUrlInternal(const std::string& url) override;
    bool loadHtmlInternal(const std::string& html, const std::string& baseUrl) override;
    void executeJavaScriptInternal(const std::string& script) override;
    void drawSelf(Fw::DrawPane drawPane) override;

private:
#ifdef USE_CEF
    friend class SimpleCEFClient;
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<CefClient> m_client;
    std::string m_pendingHtml; // Store HTML to load after browser creation
    std::string m_pendingUrl;  // Store URL to load after browser creation
    
    // OTClient Texture for CEF rendering
    TexturePtr m_cefTexture;
    ImagePtr m_cefImage;
    bool m_textureCreated;
    int m_lastWidth;
    int m_lastHeight;
    Point m_lastMousePos;
    
    // Static tracking of all active WebViews
    static std::vector<UICEFWebView*> s_activeWebViews;
#endif
}; 