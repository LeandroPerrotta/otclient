#pragma once

#include "uiwebview.h"

#ifdef USE_CEF
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include <framework/graphics/texture.h>

// Forward declaration
class SimpleCEFClient;
#endif

class UICEFWebView : public UIWebView {
public:
    UICEFWebView();
    UICEFWebView(UIWidgetPtr parent);
    virtual ~UICEFWebView();

    // CEF-specific methods
    void onCEFPaint(const void* buffer, int width, int height);
    void onBrowserCreated(CefRefPtr<CefBrowser> browser);
    
    // Static methods for managing all WebViews
    static void closeAllWebViews();
    static size_t getActiveWebViewCount();

    // Event handlers
    void onLoadStarted() override;
    void onLoadFinished(bool success) override;
    void onUrlChanged(const std::string& url) override;
    void onTitleChanged(const std::string& title) override;
    void onJavaScriptCallback(const std::string& name, const std::string& data) override;

protected:
    void createWebView() override;
    void loadUrlInternal(const std::string& url) override;
    bool loadHtmlInternal(const std::string& html, const std::string& baseUrl) override;
    void drawSelf(Fw::DrawPane drawPane) override;

private:
#ifdef USE_CEF
    friend class SimpleCEFClient;
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<CefClient> m_client;
    std::string m_pendingHtml; // Store HTML to load after browser creation
    
    // OTClient Texture for CEF rendering
    TexturePtr m_cefTexture;
    bool m_textureCreated;
    int m_lastWidth;
    int m_lastHeight;
    
    // Static tracking of all active WebViews
    static std::vector<UICEFWebView*> s_activeWebViews;
#endif
}; 