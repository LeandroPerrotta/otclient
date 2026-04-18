#include "cef_luahandler.h"
#include "uicefwebview.h"
#include <framework/core/eventdispatcher.h>
#include "include/cef_parser.h"
#include <string>

CefLuaHandler::CefLuaHandler(UICEFWebView* webview) : m_webview(webview)
{
}

bool CefLuaHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             int64_t query_id,
                             const CefString& request,
                             bool persistent,
                             CefRefPtr<Callback> callback)
{
    (void)browser;
    (void)frame;
    (void)query_id;
    (void)persistent;

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

void CefLuaHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    int64_t query_id)
{
    (void)browser;
    (void)frame;
    (void)query_id;
}

