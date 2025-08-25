#pragma once

#include <framework/ui/uiwidget.h>
#include <framework/ui/declarations.h>
#include <mutex>
#include <atomic>
#include <map>
#include <functional>

#ifdef USE_CEF
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "../graphics/cef_renderer.h"

// Forward declaration
class SimpleCEFClient;
#endif

// @bindclass
class UICEFWebView : public UIWidget {
public:
    UICEFWebView();
    UICEFWebView(UIWidgetPtr parent);
    virtual ~UICEFWebView();

    // WebView interface
    void loadUrl(const std::string& url);
    void loadHtml(const std::string& html, const std::string& baseUrl = "");
    void loadFile(const std::string& filePath);

    void executeJavaScript(const std::string& script);
    void setZoomLevel(float zoom);
    float getZoomLevel();

    void setUserAgent(const std::string& userAgent);
    std::string getUserAgent();

    void setScrollPosition(const Point& position);
    Point getScrollPosition();

    void setScrollable(bool scrollable);
    bool isScrollable();

    void goBack();
    void goForward();
    void reload();
    void stop();

    bool canGoBack();
    bool canGoForward();
    bool isLoading();

    void registerJavaScriptCallback(const std::string& name,
                                    const std::function<void(const std::string&)>& callback,
                                    int luaRef = -1);
    void unregisterJavaScriptCallback(const std::string& name);
    void sendToJavaScript(const std::string& name, const std::string& data);

    // CEF-specific methods
    void onPaint(const void* buffer, int width, int height, const CefRenderHandler::RectList& dirtyRects);
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info);
    void onBrowserCreated(CefRefPtr<CefBrowser> browser);
    
    // Static methods for managing all WebViews
    static void closeAllWebViews();
    static size_t getActiveWebViewCount();

    static void setAllWindowlessFrameRate(int fps);

    void setWindowlessFrameRate(int fps);

    // Event handlers
    virtual void onLoadStarted();
    virtual void onLoadFinished(bool success);
    virtual void onUrlChanged(const std::string& url);
    virtual void onTitleChanged(const std::string& title);
    virtual void onJavaScriptCallback(const std::string& name, const std::string& data);

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
    void drawSelf(Fw::DrawPane drawPane) override;

private:
    void createWebView();
    void loadUrlInternal(const std::string& url);
    bool loadHtmlInternal(const std::string& html, const std::string& baseUrl);
    void executeJavaScriptInternal(const std::string& script);

#ifdef USE_CEF
    friend class SimpleCEFClient;
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<CefClient> m_client;
    std::string m_pendingHtml; // Store HTML to load after browser creation
    std::string m_pendingUrl;  // Store URL to load after browser creation
    
    // Renderer abstraction
    std::unique_ptr<CefRenderer> m_renderer;
    Point m_lastMousePos;

    // Static tracking of all active WebViews (thread-safe)
    static std::vector<UICEFWebView*> s_activeWebViews;
    static std::mutex s_activeWebViewsMutex;

    // Instance validity flag (for scheduled events)
    std::atomic<bool> m_isValid;

    // State and JavaScript callbacks
    std::string m_currentUrl;
    std::string m_currentTitle;
    std::string m_userAgent;
    float m_zoomLevel;
    Point m_scrollPosition;
    bool m_scrollable;
    bool m_loading;

    struct JSCallback {
        std::function<void(const std::string&)> callback;
        int luaRef;
    };
    std::map<std::string, JSCallback> m_jsCallbacks;

#endif
};
