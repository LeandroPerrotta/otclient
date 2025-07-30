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

#include "uiwebview.h"
#include <framework/core/resourcemanager.h>
#include <framework/core/application.h>

UIWebView::UIWebView()
    : m_webViewHandle(nullptr)
    , m_zoomLevel(1.0f)
    , m_scrollable(true)
    , m_loading(false)
{
    m_userAgent = "OTClient WebView/1.0";
    initializeWebView();
}

UIWebView::UIWebView(UIWidgetPtr parent)
    : UIWidget()
    , m_webViewHandle(nullptr)
    , m_zoomLevel(1.0f)
    , m_scrollable(true)
    , m_loading(false)
{
    m_userAgent = "OTClient WebView/1.0";
    if (parent) {
        setParent(parent);
    }
    initializeWebView();
}

UIWebView::~UIWebView()
{
    cleanupWebView();
}

void UIWebView::loadUrl(const std::string& url)
{
    m_currentUrl = url;
    m_loading = true;
    onLoadStarted();
    loadUrlInternal(url);
}

void UIWebView::loadHtml(const std::string& html, const std::string& baseUrl)
{
    m_loading = true;
    onLoadStarted();
    loadHtmlInternal(html, baseUrl);
}

void UIWebView::loadFile(const std::string& filePath)
{
    std::string fullPath = g_resources.guessFilePath(filePath, "html");
    if (fullPath.empty()) {
        fullPath = g_resources.guessFilePath(filePath, "htm");
    }
    
    if (!fullPath.empty()) {
        std::string html = g_resources.readFileContents(fullPath);
        if (!html.empty()) {
            loadHtml(html, "file://" + fullPath);
        }
    }
}

void UIWebView::executeJavaScript(const std::string& script)
{
    executeJavaScriptInternal(script);
}

void UIWebView::setZoomLevel(float zoom)
{
    m_zoomLevel = std::max(0.25f, std::min(5.0f, zoom));
    setZoomLevelInternal(m_zoomLevel);
}

float UIWebView::getZoomLevel()
{
    return m_zoomLevel;
}

void UIWebView::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
    setUserAgentInternal(userAgent);
}

std::string UIWebView::getUserAgent()
{
    return m_userAgent;
}

void UIWebView::setScrollPosition(const Point& position)
{
    m_scrollPosition = position;
    setScrollPositionInternal(position);
}

Point UIWebView::getScrollPosition()
{
    return m_scrollPosition;
}

void UIWebView::setScrollable(bool scrollable)
{
    m_scrollable = scrollable;
}

bool UIWebView::isScrollable()
{
    return m_scrollable;
}

void UIWebView::goBack()
{
    if (canGoBack()) {
        goBackInternal();
    }
}

void UIWebView::goForward()
{
    if (canGoForward()) {
        goForwardInternal();
    }
}

void UIWebView::reload()
{
    reloadInternal();
}

void UIWebView::stop()
{
    stopInternal();
}

bool UIWebView::canGoBack()
{
    return canGoBackInternal();
}

bool UIWebView::canGoForward()
{
    return canGoForwardInternal();
}

bool UIWebView::isLoading()
{
    return m_loading;
}

void UIWebView::registerJavaScriptCallback(const std::string& name, const std::function<void(const std::string&)>& callback)
{
    m_jsCallbacks[name] = callback;
}

void UIWebView::unregisterJavaScriptCallback(const std::string& name)
{
    m_jsCallbacks.erase(name);
}

void UIWebView::onLoadStarted()
{
    // Override in derived classes if needed
}

void UIWebView::onLoadFinished(bool success)
{
    m_loading = false;
    // Override in derived classes if needed
}

void UIWebView::onUrlChanged(const std::string& url)
{
    m_currentUrl = url;
    // Override in derived classes if needed
}

void UIWebView::onTitleChanged(const std::string& title)
{
    m_currentTitle = title;
    // Override in derived classes if needed
}

void UIWebView::onJavaScriptCallback(const std::string& name, const std::string& data)
{
    auto it = m_jsCallbacks.find(name);
    if (it != m_jsCallbacks.end()) {
        it->second(data);
    }
}

void UIWebView::drawSelf(Fw::DrawPane drawPane)
{
    // WebView content is rendered by the native WebView engine
    // This method can be overridden to add custom drawing on top
    UIWidget::drawSelf(drawPane);
}

void UIWebView::onGeometryChange(const Rect& oldRect, const Rect& newRect)
{
    UIWidget::onGeometryChange(oldRect, newRect);
    
    if (oldRect.size() != newRect.size()) {
        updateWebViewSize();
    }
}

void UIWebView::updateWebViewSize()
{
    if (m_webViewHandle) {
        resizeWebView(getSize());
    }
}

void UIWebView::initializeWebView()
{
    if (!m_webViewHandle) {
        createWebView();
        updateWebViewSize();
    }
}

void UIWebView::cleanupWebView()
{
    if (m_webViewHandle) {
        destroyWebView();
        m_webViewHandle = nullptr;
    }
}

// Default implementations for virtual methods
void UIWebView::createWebView()
{
    g_logger.info("UIWebView: createWebView() - No WebView engine available");
}

void UIWebView::destroyWebView()
{
    g_logger.info("UIWebView: destroyWebView() - No WebView engine available");
}

void UIWebView::resizeWebView(const Size& size)
{
    g_logger.info("UIWebView: resizeWebView() - No WebView engine available");
}

void UIWebView::loadUrlInternal(const std::string& url)
{
    g_logger.warning("UIWebView: Cannot load URL, no WebView engine available");
}

bool UIWebView::loadHtmlInternal(const std::string& html, const std::string& baseUrl)
{
    g_logger.warning("UIWebView: Cannot load HTML, no WebView engine available");
}

void UIWebView::executeJavaScriptInternal(const std::string& script)
{
    g_logger.warning("UIWebView: Cannot execute JavaScript, no WebView engine available");
}

void UIWebView::setZoomLevelInternal(float zoom)
{
    g_logger.info("UIWebView: setZoomLevelInternal() - No WebView engine available");
}

void UIWebView::setUserAgentInternal(const std::string& userAgent)
{
    g_logger.info("UIWebView: setUserAgentInternal() - No WebView engine available");
}

void UIWebView::setScrollPositionInternal(const Point& position)
{
    g_logger.info("UIWebView: setScrollPositionInternal() - No WebView engine available");
}

void UIWebView::goBackInternal()
{
    g_logger.info("UIWebView: goBackInternal() - No WebView engine available");
}

void UIWebView::goForwardInternal()
{
    g_logger.info("UIWebView: goForwardInternal() - No WebView engine available");
}

void UIWebView::reloadInternal()
{
    g_logger.info("UIWebView: reloadInternal() - No WebView engine available");
}

void UIWebView::stopInternal()
{
    g_logger.info("UIWebView: stopInternal() - No WebView engine available");
}

bool UIWebView::canGoBackInternal()
{
    return false;
}

bool UIWebView::canGoForwardInternal()
{
    return false;
}

bool UIWebView::isLoadingInternal()
{
    return false;
}

Point UIWebView::getScrollPositionInternal()
{
    return Point(0, 0);
}

float UIWebView::getZoomLevelInternal()
{
    return 1.0f;
}

std::string UIWebView::getUserAgentInternal()
{
    return "OTClient WebView/1.0 (No Engine)";
} 