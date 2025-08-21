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

#ifndef UIWEBVIEW_H
#define UIWEBVIEW_H

#include "uiwidget.h"
#include <framework/core/declarations.h>

// @bindclass
class UIWebView : public UIWidget
{
public:
    UIWebView();
    UIWebView(UIWidgetPtr parent);
    virtual ~UIWebView();

    // WebView specific methods
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
    
    // Navigation
    void goBack();
    void goForward();
    void reload();
    void stop();
    
    bool canGoBack();
    bool canGoForward();
    bool isLoading();
    
    // JavaScript bridge
    void registerJavaScriptCallback(const std::string& name,
                                    const std::function<void(const std::string&)>& callback,
                                    int luaRef = -1);
    void unregisterJavaScriptCallback(const std::string& name);
    void sendToJavaScript(const std::string& name, const std::string& data);
    
    // Events
    virtual void onLoadStarted();
    virtual void onLoadFinished(bool success);
    virtual void onUrlChanged(const std::string& url);
    virtual void onTitleChanged(const std::string& title);
    virtual void onJavaScriptCallback(const std::string& name, const std::string& data);

protected:
    virtual void drawSelf(Fw::DrawPane drawPane) override;
    virtual void onGeometryChange(const Rect& oldRect, const Rect& newRect) override;
    
    // WebView implementation (to be implemented by specific WebView engine)
    virtual void createWebView();
    virtual void destroyWebView();
    virtual void resizeWebView(const Size& size);
    virtual void loadUrlInternal(const std::string& url);
    virtual bool loadHtmlInternal(const std::string& html, const std::string& baseUrl);
    virtual void executeJavaScriptInternal(const std::string& script);
    virtual void setZoomLevelInternal(float zoom);
    virtual void setUserAgentInternal(const std::string& userAgent);
    virtual void setScrollPositionInternal(const Point& position);
    virtual void goBackInternal();
    virtual void goForwardInternal();
    virtual void reloadInternal();
    virtual void stopInternal();
    virtual bool canGoBackInternal();
    virtual bool canGoForwardInternal();
    virtual bool isLoadingInternal();
    virtual Point getScrollPositionInternal();
    virtual float getZoomLevelInternal();
    virtual std::string getUserAgentInternal();

protected:
    void* m_webViewHandle; // Platform-specific WebView handle

private:
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
    
    void updateWebViewSize();
    void initializeWebView();
    void cleanupWebView();
};

#endif 