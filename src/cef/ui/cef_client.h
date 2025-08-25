#pragma once

#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/wrapper/cef_message_router.h"
#include "include/cef_base.h"
#include <memory>

class UICEFWebView;
class CefLuaHandler;

// Simple CEF Client implementation
class SimpleCEFClient : public CefClient,
                        public CefRenderHandler,
                        public CefRequestHandler,
                        public CefLifeSpanHandler {
public:
    explicit SimpleCEFClient(UICEFWebView* webview);
    ~SimpleCEFClient() override;

    CefRefPtr<CefRenderHandler> GetRenderHandler() override;
    CefRefPtr<CefRequestHandler> GetRequestHandler() override;
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override;

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override;

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override;

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;

    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override;

    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const CefAcceleratedPaintInfo& info) override;

private:
    UICEFWebView* m_webview;
    CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;
    std::unique_ptr<CefLuaHandler> m_luaHandler;

    IMPLEMENT_REFCOUNTING(SimpleCEFClient);
};

