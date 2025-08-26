#pragma once

#include <cstdint>
#include "include/cef_base.h"
#include "include/wrapper/cef_message_router.h"

class UICEFWebView;

class CefLuaHandler : public CefMessageRouterBrowserSide::Handler {
public:
    explicit CefLuaHandler(UICEFWebView* webview);

    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64_t query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override;

    void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64_t query_id) override;

private:
    UICEFWebView* m_webview;
};

