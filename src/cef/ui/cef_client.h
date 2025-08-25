#pragma once

#include "uicefwebview.h"
#include <cef/resources/cefphysfsresourcehandler.h>
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/wrapper/cef_message_router.h"
#include "include/cef_parser.h"
#include <vector>
#include <memory>
#include <cstring>

// Simple CEF Client implementation
class SimpleCEFClient : public CefClient,
                        public CefRenderHandler,
                        public CefRequestHandler,
                        public CefLifeSpanHandler {
public:
    explicit SimpleCEFClient(UICEFWebView* webview) : m_webview(webview) {
        CefMessageRouterConfig config;
        config.js_query_function = "luaCallback";
        config.js_cancel_function = "luaCallbackCancel";
        m_messageRouter = CefMessageRouterBrowserSide::Create(config);
        m_routerHandler = std::make_unique<LuaCallbackHandler>(m_webview);
        m_messageRouter->AddHandler(m_routerHandler.get(), false);
    }

    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override {
        std::string url = request->GetURL();
        if (url.rfind("otclient://", 0) == 0 ||
            url.rfind("http://otclient/", 0) == 0 ||
            url.rfind("https://otclient/", 0) == 0) {
            return new CefPhysFsResourceRequestHandler();
        }
        return nullptr;
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override {
        if (m_messageRouter &&
            m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message))
            return true;
        return false;
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override {
        if (m_messageRouter)
            m_messageRouter->OnBeforeBrowse(browser, frame);
        return false;
    }

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_logger.info("UICEFWebView: OnAfterCreated called - browser is ready!");
        if (m_webview) {
            m_webview->onBrowserCreated(browser);
        }
    }

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        rect.x = rect.y = 0;
        if (m_webview) {
            Size widgetSize = m_webview->getSize();
            rect.width = widgetSize.width();
            rect.height = widgetSize.height();
        } else {
            rect.width = 600;
            rect.height = 400;
        }
    }

    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override {
        static bool sofwareAccelerationLogged = false;
        if (!sofwareAccelerationLogged) {
            g_logger.info("============ UICEFWebView: Software acceleration is enabled =============");
            g_logger.info("UICEFWebView: OnPaint called - using software rendering");
            sofwareAccelerationLogged = true;
        }

        if (m_webview) {
            if (!m_webview->m_browser) {
                m_webview->onBrowserCreated(browser);
            }
            if (type == PET_VIEW) {
                // With multi_threaded_message_loop = true, we're on CEF UI thread
                // Need to schedule paint processing on main thread for OpenGL context

                // Copy buffer data since it may be freed after this callback
                std::vector<uint8_t> bufferCopy(width * height * 4);
                memcpy(bufferCopy.data(), buffer, bufferCopy.size());

                g_dispatcher.scheduleEvent([webview = m_webview, bufferCopy = std::move(bufferCopy), width, height, dirtyRects](
) mutable {
                    if (webview) {
                        webview->onCEFPaint(bufferCopy.data(), width, height, dirtyRects);
                    }
                }, 0);
            }
        }
    }

    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const CefAcceleratedPaintInfo& info) override {
        static bool gpuAccelerationLogged = false;
        if (!gpuAccelerationLogged) {
            g_logger.info("============ UICEFWebView: GPU acceleration is enabled =============");
            g_logger.info("UICEFWebView: OnAcceleratedPaint called - using GPU rendering!");
            g_logger.info("UICEFWebView: accelerated paint info received");
            gpuAccelerationLogged = true;
        }

        if (m_webview) {
            if (!m_webview->m_browser) {
                g_logger.info("UICEFWebView: Browser created via OnAcceleratedPaint callback");
                m_webview->onBrowserCreated(browser);
            }
            if (type == PET_VIEW) {
                // CRITICAL: Must process immediately! CEF releases the resource after this callback returns
                if (m_webview) {
                    m_webview->onCEFAcceleratedPaint(info);
                }
            }
        }
    }

private:
    class LuaCallbackHandler : public CefMessageRouterBrowserSide::Handler {
    public:
        explicit LuaCallbackHandler(UICEFWebView* webview) : m_webview(webview) {}

        bool OnQuery(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     int64 query_id,
                     const CefString& request,
                     bool persistent,
                     CefRefPtr<Callback> callback) override {
            CefRefPtr<CefValue> value = CefParseJSON(request, JSON_PARSER_RFC);
            if (value && value->GetType() == VTYPE_DICTIONARY) {
                CefRefPtr<CefDictionaryValue> dict = value->GetDictionary();
                std::string name = dict->GetString("name");
                std::string data;
                if (dict->HasKey("data")) {
                    CefRefPtr<CefValue> dataValue = dict->GetValue("data");
                    switch (dataValue->GetType()) {
                        case VTYPE_STRING:
                            // String: usar diretamente
                            data = dataValue->GetString();
                            break;
                        case VTYPE_INT:
                            // Inteiro: converter para string
                            data = std::to_string(dataValue->GetInt());
                            break;
                        case VTYPE_DOUBLE:
                            // Double: converter para string
                            data = std::to_string(dataValue->GetDouble());
                            break;
                        case VTYPE_BOOL:
                            // Boolean: converter para string
                            data = dataValue->GetBool() ? "true" : "false";
                            break;
                        default:
                            // Para tipos complexos (array, object, null), usar JSON
                            data = CefWriteJSON(dataValue, JSON_WRITER_DEFAULT).ToString();
                            break;
                    }
                }
                if (m_webview && !name.empty()) {
                    // With multi_threaded_message_loop = true, we're on CEF UI thread
                    // Need to schedule callback on main thread for thread safety
                    g_dispatcher.addEventFromOtherThread([webview = m_webview, name, data]() {
                        if (webview) {
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
                             int64 query_id) override {}

    private:
        UICEFWebView* m_webview;
    };

    UICEFWebView* m_webview;
    CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;
    std::unique_ptr<LuaCallbackHandler> m_routerHandler;

    IMPLEMENT_REFCOUNTING(SimpleCEFClient);
};

