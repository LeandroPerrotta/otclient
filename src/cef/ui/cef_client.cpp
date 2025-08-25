#include "cef_client.h"
#include "uicefwebview.h"
#include <cef/resources/cefphysfsresourcehandler.h>
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <vector>
#include <cstring>
#include "cef_luahandler.h"

SimpleCEFClient::SimpleCEFClient(UICEFWebView* webview) : m_webview(webview)
{
    CefMessageRouterConfig config;
    config.js_query_function = "luaCallback";
    config.js_cancel_function = "luaCallbackCancel";
    m_messageRouter = CefMessageRouterBrowserSide::Create(config);
    m_luaHandler = std::make_unique<CefLuaHandler>(m_webview);
    m_messageRouter->AddHandler(m_luaHandler.get(), false);
}

SimpleCEFClient::~SimpleCEFClient() = default;

CefRefPtr<CefRenderHandler> SimpleCEFClient::GetRenderHandler()
{
    return this;
}

CefRefPtr<CefRequestHandler> SimpleCEFClient::GetRequestHandler()
{
    return this;
}

CefRefPtr<CefLifeSpanHandler> SimpleCEFClient::GetLifeSpanHandler()
{
    return this;
}

CefRefPtr<CefResourceRequestHandler> SimpleCEFClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling)
{
    std::string url = request->GetURL();
    if(url.rfind("otclient://", 0) == 0 ||
       url.rfind("http://otclient/", 0) == 0 ||
       url.rfind("https://otclient/", 0) == 0) {
        return new CefPhysFsResourceRequestHandler();
    }
    return nullptr;
}

bool SimpleCEFClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                              CefRefPtr<CefFrame> frame,
                                              CefProcessId source_process,
                                              CefRefPtr<CefProcessMessage> message)
{
    if(m_messageRouter &&
       m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message))
        return true;
    return false;
}

bool SimpleCEFClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefRequest> request,
                                     bool user_gesture,
                                     bool is_redirect)
{
    if(m_messageRouter)
        m_messageRouter->OnBeforeBrowse(browser, frame);
    return false;
}

void SimpleCEFClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    g_logger.info("UICEFWebView: OnAfterCreated called - browser is ready!");
    if(m_webview) {
        m_webview->onBrowserCreated(browser);
    }
}

void SimpleCEFClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    rect.x = rect.y = 0;
    if(m_webview) {
        Size widgetSize = m_webview->getSize();
        rect.width = widgetSize.width();
        rect.height = widgetSize.height();
    } else {
        rect.width = 600;
        rect.height = 400;
    }
}

void SimpleCEFClient::OnPaint(CefRefPtr<CefBrowser> browser,
                              PaintElementType type,
                              const RectList& dirtyRects,
                              const void* buffer,
                              int width, int height)
{
    static bool sofwareAccelerationLogged = false;
    if(!sofwareAccelerationLogged) {
        g_logger.info("============ UICEFWebView: Software acceleration is enabled =============");
        g_logger.info("UICEFWebView: OnPaint called - using software rendering");
        sofwareAccelerationLogged = true;
    }

    if(m_webview) {
        if(!m_webview->m_browser) {
            m_webview->onBrowserCreated(browser);
        }
        if(type == PET_VIEW) {
            std::vector<uint8_t> bufferCopy(width * height * 4);
            memcpy(bufferCopy.data(), buffer, bufferCopy.size());

            g_dispatcher.scheduleEvent([webview = m_webview, bufferCopy = std::move(bufferCopy), width, height, dirtyRects]() mutable {
                if(webview) {
                    webview->onPaint(bufferCopy.data(), width, height, dirtyRects);
                }
            }, 0);
        }
    }
}

void SimpleCEFClient::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                         PaintElementType type,
                                         const RectList& dirtyRects,
                                         const CefAcceleratedPaintInfo& info)
{
    static bool gpuAccelerationLogged = false;
    if(!gpuAccelerationLogged) {
        g_logger.info("============ UICEFWebView: GPU acceleration is enabled =============");
        g_logger.info("UICEFWebView: OnAcceleratedPaint called - using GPU rendering!");
        g_logger.info("UICEFWebView: accelerated paint info received");
        gpuAccelerationLogged = true;
    }

    if(m_webview) {
        if(!m_webview->m_browser) {
            g_logger.info("UICEFWebView: Browser created via OnAcceleratedPaint callback");
            m_webview->onBrowserCreated(browser);
        }
        if(type == PET_VIEW) {
            m_webview->onAcceleratedPaint(info);
        }
    }
}


