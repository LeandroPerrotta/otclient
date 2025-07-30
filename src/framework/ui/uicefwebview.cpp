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
#include <framework/graphics/graphics.h>
#include <framework/graphics/image.h>
#include <unistd.h>
#include <string>
#include <vector>

#ifdef USE_CEF
#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/cef_browser.h>
#include <include/wrapper/cef_helpers.h>
#include "include/cef_parser.h"
#include <GL/gl.h>

std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
               .ToString();
}

// Global flag to check if CEF was initialized
extern bool g_cefInitialized;

// Static member initialization
std::vector<UICEFWebView*> UICEFWebView::s_activeWebViews;

// Simple CEF Client implementation
class SimpleCEFClient : public CefClient, public CefRenderHandler {
public:
    SimpleCEFClient(UICEFWebView* webview) : m_webview(webview) {}

    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }

    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        rect.x = rect.y = 0;
        rect.width = 400;
        rect.height = 300;
    }

    virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                        PaintElementType type,
                        const RectList& dirtyRects,
                        const void* buffer,
                        int width, int height) override {
        g_logger.info("UICEFWebView: OnPaint called - type: " + std::to_string(type) + ", size: " + std::to_string(width) + "x" + std::to_string(height));
        
        if (m_webview) {
            // Capture browser reference on first paint
            if (!m_webview->m_browser) {
                g_logger.info("UICEFWebView: First paint - capturing browser reference");
                m_webview->onBrowserCreated(browser);
            }
            
            if (type == PET_VIEW) {
                g_logger.info("UICEFWebView: PET_VIEW paint - calling onCEFPaint");
                m_webview->onCEFPaint(buffer, width, height);
            }
        }
    }

private:
    UICEFWebView* m_webview;
    IMPLEMENT_REFCOUNTING(SimpleCEFClient);
};

UICEFWebView::UICEFWebView()
    : UIWebView(), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0)
{
    g_logger.info("UICEFWebView: Constructor called - CEF initialized: " + std::to_string(g_cefInitialized));
    setSize(Size(800, 600)); // Set initial size
    s_activeWebViews.push_back(this);
    g_logger.info("UICEFWebView: Added to active list. Total active: " + std::to_string(s_activeWebViews.size()));
}

UICEFWebView::UICEFWebView(UIWidgetPtr parent)
    : UIWebView(parent), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0)
{
    g_logger.info("UICEFWebView: Constructor with parent called - CEF initialized: " + std::to_string(g_cefInitialized));
    setSize(Size(800, 600)); // Set initial size
    s_activeWebViews.push_back(this);
    g_logger.info("UICEFWebView: Added to active list. Total active: " + std::to_string(s_activeWebViews.size()));
}

UICEFWebView::~UICEFWebView()
{
    g_logger.info("UICEFWebView: Destructor called");
    
    // Remove from active list
    auto it = std::find(s_activeWebViews.begin(), s_activeWebViews.end(), this);
    if (it != s_activeWebViews.end()) {
        s_activeWebViews.erase(it);
        g_logger.info("UICEFWebView: Removed from active list. Remaining: " + std::to_string(s_activeWebViews.size()));
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

    // Window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0); // 0 = no parent window
    g_logger.info("UICEFWebView: Window info configured for off-screen rendering");

    // Create browser asynchronously
    bool result = CefBrowserHost::CreateBrowser(window_info, m_client, "about:blank", browser_settings, nullptr, nullptr);
    g_logger.info("UICEFWebView: CreateBrowser called, result: " + std::to_string(result));
    
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
        if (!m_browser) {
            g_logger.error("Cannot load URL: browser not created");
            return;
        }
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
    g_logger.info("UICEFWebView: Data URL length: " + std::to_string(html.length()));
    g_logger.info("UICEFWebView: HTML content: " + html.substr(0, 100) + "...");
    m_browser->GetMainFrame()->LoadURL(dataUri);

    return true;
}

void UICEFWebView::onCEFPaint(const void* buffer, int width, int height)
{
    g_logger.info("UICEFWebView: Paint callback - " + std::to_string(width) + "x" + std::to_string(height));
    
    // Convert BGRA to RGBA and create OTClient Texture
    if (buffer && width > 0 && height > 0) {
        // Debug: Check first few pixels of CEF buffer
        const uint8_t* bgra = static_cast<const uint8_t*>(buffer);
        g_logger.info("UICEFWebView: CEF buffer first pixel BGRA: " + 
                     std::to_string(bgra[0]) + "," + std::to_string(bgra[1]) + "," + 
                     std::to_string(bgra[2]) + "," + std::to_string(bgra[3]));
        
        // Allocate buffer for RGBA data
        std::vector<uint8_t> rgbaBuffer(width * height * 4);
        
        // Convert BGRA to RGBA
        for (int i = 0; i < width * height; ++i) {
            rgbaBuffer[i * 4 + 0] = bgra[i * 4 + 2]; // B -> R
            rgbaBuffer[i * 4 + 1] = bgra[i * 4 + 1]; // G -> G
            rgbaBuffer[i * 4 + 2] = bgra[i * 4 + 0]; // R -> B
            rgbaBuffer[i * 4 + 3] = bgra[i * 4 + 3]; // A -> A
        }
        
        // Create or update OTClient Texture
        if (!m_textureCreated || width != m_lastWidth || height != m_lastHeight) {
            // Create new Image from RGBA data
            ImagePtr image = ImagePtr(new Image(Size(width, height), 4));
            memcpy(image->getPixelData(), rgbaBuffer.data(), rgbaBuffer.size());
            
            // Create new Texture
            m_cefTexture = TexturePtr(new Texture(image));
            m_textureCreated = true;
            m_lastWidth = width;
            m_lastHeight = height;

            image->savePNG("cef_texture.png");
            
            g_logger.info("UICEFWebView: Created new texture (" + std::to_string(width) + "x" + std::to_string(height) + ") - Image size: " + std::to_string(image->getSize().width()) + "x" + std::to_string(image->getSize().height()));
        } else {
            // Update existing texture
            ImagePtr image = ImagePtr(new Image(Size(width, height), 4));
            memcpy(image->getPixelData(), rgbaBuffer.data(), rgbaBuffer.size());
            m_cefTexture->uploadPixels(image);
            
            g_logger.info("UICEFWebView: Updated existing texture (" + std::to_string(width) + "x" + std::to_string(height) + ")");
        }
        
        // Set widget size to match texture
        setSize(Size(width, height));
        setVisible(true);
        
        g_logger.info("UICEFWebView: Texture updated successfully");
    }
}

void UICEFWebView::onBrowserCreated(CefRefPtr<CefBrowser> browser)
{
    g_logger.info("UICEFWebView: Browser created successfully!");
    m_browser = browser;
    
    if (!m_pendingHtml.empty()) {
        g_logger.info("UICEFWebView: Loading pending HTML content...");
        g_logger.info("UICEFWebView: HTML content length: " + std::to_string(m_pendingHtml.length()));
        g_logger.info("UICEFWebView: HTML preview: " + m_pendingHtml.substr(0, 200) + "...");

        if(!loadHtmlInternal(m_pendingHtml, "")) {
            g_logger.error("UICEFWebView: Failed to load pending HTML content");
        }
        
        m_pendingHtml.clear();
    }
}

void UICEFWebView::drawSelf(Fw::DrawPane drawPane)
{
    //g_logger.info("UICEFWebView: drawSelf called - textureCreated: " + std::to_string(m_textureCreated) + ", texture: " + (m_cefTexture ? "valid" : "null"));
    
    if (m_textureCreated && m_cefTexture) {
        // Debug texture info
        // g_logger.info("UICEFWebView: Texture ID: " + std::to_string(m_cefTexture->getId()) + ", Size: " + std::to_string(m_cefTexture->getWidth()) + "x" + std::to_string(m_cefTexture->getHeight()));
        
        // Debug widget info
        Rect rect = getRect();
        // g_logger.info("UICEFWebView: Widget rect: " + std::to_string(rect.x()) + "," + std::to_string(rect.y()) + " " + std::to_string(rect.width()) + "x" + std::to_string(rect.height()));
        // g_logger.info("UICEFWebView: Widget visible: " + std::to_string(isVisible()) + ", enabled: " + std::to_string(isEnabled()));
        
        // // Test: Draw a solid blue background first to verify rendering pipeline
        //g_painter->setColor(Color::blue);
        g_painter->drawFilledRect(rect);
        // g_logger.info("UICEFWebView: Drew blue background for testing");
        
        // // Render the CEF texture using OTClient's graphics system
        g_painter->setOpacity(1.0);
        
        g_painter->drawTexturedRect(rect, m_cefTexture);
        
        //g_logger.info("UICEFWebView: Rendering CEF texture (" + std::to_string(rect.width()) + "x" + std::to_string(rect.height()) + ") - Texture ID: " + std::to_string(m_cefTexture->getId()));
    } else {
        // Fallback to default rendering with visible background
        // UIWidget::drawSelf(drawPane);
        g_logger.info("UICEFWebView: Using fallback rendering (red background)");
    }
}

// Event handlers with correct signatures
void UICEFWebView::onLoadStarted() {}
void UICEFWebView::onLoadFinished(bool success) {}
void UICEFWebView::onUrlChanged(const std::string& url) {}
void UICEFWebView::onTitleChanged(const std::string& title) {}
void UICEFWebView::onJavaScriptCallback(const std::string& name, const std::string& data) {}

// Static methods for managing all WebViews
void UICEFWebView::closeAllWebViews() {
    g_logger.info("UICEFWebView: Closing all active WebViews. Count: " + std::to_string(s_activeWebViews.size()));
    
    // Create a copy of the vector to avoid issues during iteration
    std::vector<UICEFWebView*> webViewsToClose = s_activeWebViews;
    
    for (auto* webview : webViewsToClose) {
        if (webview && webview->m_browser) {
            g_logger.info("UICEFWebView: Force closing browser");
            webview->m_browser->GetHost()->CloseBrowser(true);
            webview->m_browser = nullptr;
        }
    }
    
    s_activeWebViews.clear();
    g_logger.info("UICEFWebView: All WebViews closed");
}

size_t UICEFWebView::getActiveWebViewCount() {
    return s_activeWebViews.size();
}

#endif // USE_CEF 