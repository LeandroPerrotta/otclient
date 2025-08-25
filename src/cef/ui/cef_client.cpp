#include "cef_client.h"
#include "uicefwebview.h"
#include <cef/resources/cefphysfsresourcehandler.h>
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include "include/cef_parser.h"
#include <vector>
#include <cstring>

class LuaCallbackHandler : public CefMessageRouterBrowserSide::Handler {
public:
    explicit LuaCallbackHandler(UICEFWebView* webview) : m_webview(webview) {}

    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64 query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override
    {
        CefRefPtr<CefValue> value = CefParseJSON(request, JSON_PARSER_RFC);
        if(value && value->GetType() == VTYPE_DICTIONARY) {
            CefRefPtr<CefDictionaryValue> dict = value->GetDictionary();
            std::string name = dict->GetString("name");
            std::string data;
            if(dict->HasKey("data")) {
                CefRefPtr<CefValue> dataValue = dict->GetValue("data");
                switch(dataValue->GetType()) {
                    case VTYPE_STRING:
                        data = dataValue->GetString();
                        break;
                    case VTYPE_INT:
                        data = std::to_string(dataValue->GetInt());
                        break;
                    case VTYPE_DOUBLE:
                        data = std::to_string(dataValue->GetDouble());
                        break;
                    case VTYPE_BOOL:
                        data = dataValue->GetBool() ? "true" : "false";
                        break;
                    default:
                        data = CefWriteJSON(dataValue, JSON_WRITER_DEFAULT).ToString();
                        break;
                }
            }
            if(m_webview && !name.empty()) {
                g_dispatcher.addEventFromOtherThread([webview = m_webview, name, data]() {
                    if(webview) {
                        webview->onJavaScriptCallback(name, data);
                    }
                });
                callback->Success("");
                return true;
            }
        }
        callback->Failure(0, "Invalid request");
        return true;
    }

    void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64 query_id) override
    {
        (void)browser;
        (void)frame;
        (void)query_id;
    }

private:
    UICEFWebView* m_webview;
};

SimpleCEFClient::SimpleCEFClient(UICEFWebView* webview) : m_webview(webview)
{
    CefMessageRouterConfig config;
    config.js_query_function = "luaCallback";
    config.js_cancel_function = "luaCallbackCancel";
    m_messageRouter = CefMessageRouterBrowserSide::Create(config);
    m_routerHandler = std::make_unique<LuaCallbackHandler>(m_webview);
    m_messageRouter->AddHandler(m_routerHandler.get(), false);
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


