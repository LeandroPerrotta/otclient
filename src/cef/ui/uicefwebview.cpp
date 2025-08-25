/*
 * Copyright (c) 2010-2020 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/core/clock.h>
#include <framework/core/graphicalapplication.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include "../graphics/cef_rendererfactory.h"
#include "cef_client.h"
#include "cef_inputhandler.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <memory>

#ifdef USE_CEF
#include <include/cef_app.h>
#include <include/cef_render_handler.h>
#include <include/cef_browser.h>
#include <include/cef_request_handler.h>
#include <include/cef_resource_request_handler.h>
#include <include/cef_life_span_handler.h>
#include <include/wrapper/cef_message_router.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_frame.h>
#include "include/cef_parser.h"
#ifdef _WIN32
#include <d3d11.h>
#include <dxgi.h>
#endif
#include <cef/resources/cefphysfsresourcehandler.h>
#include <thread>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>

// typedef (geralmente já vem de gl2ext.h, mas garantimos)
#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC_DEFINED
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum, GLeglImageOES);
#define PFNGLEGLIMAGETARGETTEXTURE2DOESPROC_DEFINED
#endif

std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
               .ToString();
}

// Global flag to check if CEF was initialized
extern bool g_cefInitialized;

// Static member initialization
std::vector<UICEFWebView*> UICEFWebView::s_activeWebViews;
std::mutex UICEFWebView::s_activeWebViewsMutex;




UICEFWebView::UICEFWebView()
    : UIWebView()
    , m_browser(nullptr)
    , m_client(nullptr)
    , m_renderer(nullptr)
    , m_lastMousePos(0, 0)
    , m_isValid(true)
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    
    // Thread-safe addition to active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.push_back(this);
    }
    
    m_renderer = CefRendererFactory::createRenderer(*this);
}

UICEFWebView::UICEFWebView(UIWidgetPtr parent)
    : UIWebView(parent)
    , m_browser(nullptr)
    , m_client(nullptr)
    , m_renderer(nullptr)
    , m_lastMousePos(0, 0)
    , m_isValid(true)
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    
    // Thread-safe addition to active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.push_back(this);
    }
    
    m_renderer = CefRendererFactory::createRenderer(*this);
}

UICEFWebView::~UICEFWebView()
{
    g_logger.info("UICEFWebView: Destructor called");
    
    // Mark as invalid FIRST to prevent scheduled events from using this object
    m_isValid.store(false);
    
    // Thread-safe removal from active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        auto it = std::find(s_activeWebViews.begin(), s_activeWebViews.end(), this);
        if (it != s_activeWebViews.end()) {
            s_activeWebViews.erase(it);
            g_logger.info(stdext::format("UICEFWebView: Removed from active list. Remaining: %d", s_activeWebViews.size()));
        }
    }
    
    if (m_browser) {
        g_logger.info("UICEFWebView: Closing browser");
        m_browser->GetHost()->CloseBrowser(true);
        m_browser = nullptr;
    }
    m_client = nullptr;
    g_logger.info("UICEFWebView: Destructor completed");
}

void UICEFWebView::createWebView()
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot create browser - CEF not initialized");
        return;
    }

    g_logger.info("UICEFWebView: Creating browser...");

    // Create client
    m_client = new SimpleCEFClient(this);
    g_logger.info("UICEFWebView: Client created successfully");

    // Browser settings
    CefBrowserSettings browser_settings;
    g_logger.info("UICEFWebView: Browser settings configured");

    int maxFps = g_app.getForegroundPaneMaxFps();
    if (maxFps <= 0)
        maxFps = 60;
    browser_settings.windowless_frame_rate = maxFps;

    // Window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0); // 0 = no parent window
   
    window_info.shared_texture_enabled = true;
    // window_info.external_begin_frame_enabled = true; // Not needed with multi_threaded_message_loop = true

    g_logger.info("UICEFWebView: Window info configured for off-screen rendering");

    // Create browser asynchronously
    bool result = CefBrowserHost::CreateBrowser(window_info, m_client, "about:blank", browser_settings, nullptr, nullptr);
    g_logger.info(stdext::format("UICEFWebView: CreateBrowser called, result: %d", result));
    
    if (!result) {
        g_logger.error("Failed to create CEF browser");
        return;
    }

    g_logger.info("UICEFWebView: Browser creation initiated...");
    // Browser will be available in the first OnPaint callback
}

void UICEFWebView::loadUrlInternal(const std::string& url)
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot load URL - CEF not initialized");
        return;
    }

    if (!m_browser) {
        createWebView();
        // Store URL to load when browser is ready
        m_pendingUrl = url;
        g_logger.info("UICEFWebView: Browser creation in progress, URL will be loaded when ready");
        return;
    }

    g_logger.info("UICEFWebView: Loading URL: " + url);
    
    CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
    if (frame) {
        frame->LoadURL(url);
    }
}

bool UICEFWebView::loadHtmlInternal(const std::string& html, const std::string& baseUrl)
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot load HTML - CEF not initialized");
        return false;
    }

    if (!m_browser) {
        createWebView();
        g_logger.info("UICEFWebView: Browser creation in progress, HTML will be loaded when ready");
        m_pendingHtml = html; // Store for later loading
        return false;
    }

    g_logger.info("UICEFWebView: Loading HTML directly into browser");
    // Use a simple data URL with minimal encoding
        std::string dataUri = GetDataURI(html, "text/html");
        g_logger.info(stdext::format("UICEFWebView: Data URL length: %d", html.length()));
        g_logger.info("UICEFWebView: HTML content: " + html.substr(0, 100) + "...");
    m_browser->GetMainFrame()->LoadURL(dataUri);

    return true;
}

void UICEFWebView::executeJavaScriptInternal(const std::string& script)
{
    if (!g_cefInitialized || !m_browser)
        return;
    CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
    if (frame)
        frame->ExecuteJavaScript(script, frame->GetURL(), 0);
}

void UICEFWebView::onPaint(const void* buffer, int width, int height,
                           const CefRenderHandler::RectList& dirtyRects)
{
    if (m_renderer)
        m_renderer->onPaint(buffer, width, height, dirtyRects);
}

void UICEFWebView::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
    if (m_renderer)
        m_renderer->onAcceleratedPaint(info);
}
void UICEFWebView::onBrowserCreated(CefRefPtr<CefBrowser> browser)
{
    g_logger.info("UICEFWebView: Browser created successfully!");
    m_browser = browser;
    
    if (!m_pendingHtml.empty()) {
        g_logger.info("UICEFWebView: Loading pending HTML content...");
        g_logger.info(stdext::format("UICEFWebView: HTML content length: %d", m_pendingHtml.length()));
        g_logger.info("UICEFWebView: HTML preview: " + m_pendingHtml.substr(0, 200) + "...");

        if(!loadHtmlInternal(m_pendingHtml, "")) {
            g_logger.error("UICEFWebView: Failed to load pending HTML content");
        }
        
        m_pendingHtml.clear();
    }

    if (!m_pendingUrl.empty()) {
        g_logger.info("UICEFWebView: Loading pending URL: " + m_pendingUrl);
        CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
        if (frame) {
            frame->LoadURL(m_pendingUrl);
        }
        m_pendingUrl.clear();
    }
    
    // With multi_threaded_message_loop = true, CEF handles rendering automatically
    // No manual frame triggering needed
}

void UICEFWebView::drawSelf(Fw::DrawPane drawPane)
{
    if (m_renderer)
        m_renderer->draw(drawPane);
    else
        UIWidget::drawSelf(drawPane);
}

bool UICEFWebView::onMousePress(const Point& mousePos, Fw::MouseButton button)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMousePress(m_browser, localPos, button);

    return UIWidget::onMousePress(mousePos, button);
}

bool UICEFWebView::onMouseRelease(const Point& mousePos, Fw::MouseButton button)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMouseRelease(m_browser, localPos, button);

    return UIWidget::onMouseRelease(mousePos, button);
}

bool UICEFWebView::onMouseMove(const Point& mousePos, const Point& mouseMoved)
{
    Point localPos = mousePos - getPosition();
    m_lastMousePos = localPos;
    if (m_browser)
        CefInputHandler::handleMouseMove(m_browser, localPos, !containsPoint(mousePos));

    return UIWidget::onMouseMove(mousePos, mouseMoved);
}

bool UICEFWebView::onMouseWheel(const Point& mousePos, Fw::MouseWheelDirection direction)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMouseWheel(m_browser, localPos, direction);

    return UIWidget::onMouseWheel(mousePos, direction);
}

void UICEFWebView::onHoverChange(bool hovered)
{
    if (!hovered && m_browser)
        CefInputHandler::handleMouseMove(m_browser, m_lastMousePos, true);

    UIWidget::onHoverChange(hovered);
}

bool UICEFWebView::onKeyDown(uchar keyCode, int keyboardModifiers)
{
    if (m_browser)
        CefInputHandler::handleKeyDown(m_browser, keyCode);

    return UIWidget::onKeyDown(keyCode, keyboardModifiers);
}

bool UICEFWebView::onKeyPress(uchar keyCode, int keyboardModifiers, int autoRepeatTicks)
{
    if (m_browser && autoRepeatTicks > 0)
        CefInputHandler::handleKeyPress(m_browser, keyCode);

    return UIWidget::onKeyPress(keyCode, keyboardModifiers, autoRepeatTicks);
}

bool UICEFWebView::onKeyText(const std::string& keyText)
{
    if (m_browser && !keyText.empty())
        CefInputHandler::handleKeyText(m_browser, keyText);

    return UIWidget::onKeyText(keyText);
}

bool UICEFWebView::onKeyUp(uchar keyCode, int keyboardModifiers)
{
    if (m_browser)
        CefInputHandler::handleKeyUp(m_browser, keyCode);

    return UIWidget::onKeyUp(keyCode, keyboardModifiers);
}

// Event handlers with correct signatures
void UICEFWebView::onLoadStarted() {}
void UICEFWebView::onLoadFinished(bool success) {}
void UICEFWebView::onUrlChanged(const std::string& url) {}
void UICEFWebView::onTitleChanged(const std::string& title) {}
void UICEFWebView::onJavaScriptCallback(const std::string& name, const std::string& data) {
    UIWebView::onJavaScriptCallback(name, data);
}

void UICEFWebView::onGeometryChange(const Rect& oldRect, const Rect& newRect)
{
    UIWidget::onGeometryChange(oldRect, newRect);

    if (oldRect.size() != newRect.size() && m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host)
            host->WasResized();
    }
}

// Static methods for managing all WebViews
void UICEFWebView::closeAllWebViews() {
    std::vector<UICEFWebView*> webViewsToClose;
    
    // Thread-safe copy of the vector
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        g_logger.info(stdext::format("UICEFWebView: Closing all active WebViews. Count: %d", s_activeWebViews.size()));
        webViewsToClose = s_activeWebViews;
    }
    
    for (auto* webview : webViewsToClose) {
        if (webview && webview->m_browser) {
            g_logger.info("UICEFWebView: Force closing browser");
            webview->m_browser->GetHost()->CloseBrowser(true);
            webview->m_browser = nullptr;
        }
    }
    
    // Thread-safe clear
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.clear();
    }
    g_logger.info("UICEFWebView: All WebViews closed");
}

size_t UICEFWebView::getActiveWebViewCount() {
    std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
    return s_activeWebViews.size();
}

void UICEFWebView::setWindowlessFrameRate(int fps)
{
    if (fps <= 0)
        fps = 60;

    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host)
            host->SetWindowlessFrameRate(fps);
    }
}

void UICEFWebView::setAllWindowlessFrameRate(int fps)
{
    std::vector<UICEFWebView*> webViewsCopy;
    
    // Thread-safe copy for iteration
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        webViewsCopy = s_activeWebViews;
    }
    
    for (auto* webview : webViewsCopy) {
        if (webview)
            webview->setWindowlessFrameRate(fps);
    }
}



#endif // USE_CEF 
