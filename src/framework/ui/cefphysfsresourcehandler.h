#pragma once

#ifdef USE_CEF

#include <include/cef_resource_handler.h>
#include <include/cef_resource_request_handler.h>
#include <include/cef_request.h>
#include <include/cef_scheme.h>
#include <string>

class CefPhysFsResourceHandler : public CefResourceHandler {
public:
    explicit CefPhysFsResourceHandler(const std::string& path);

    bool ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) override;
    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64& response_length, CefString& redirectUrl) override;
    bool Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> callback) override;
    void Cancel() override;

private:
    std::string getMimeType(const std::string& ext);

    std::string m_data;
    std::string m_mimeType;
    size_t m_offset;
    IMPLEMENT_REFCOUNTING(CefPhysFsResourceHandler);
};

class CefPhysFsResourceRequestHandler : public CefResourceRequestHandler {
public:
    CefRefPtr<CefResourceHandler> GetResourceHandler(CefRefPtr<CefBrowser> browser,
                                                     CefRefPtr<CefFrame> frame,
                                                     CefRefPtr<CefRequest> request) override;

    IMPLEMENT_REFCOUNTING(CefPhysFsResourceRequestHandler);
};

class CefPhysFsSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString& scheme_name,
                                         CefRefPtr<CefRequest> request) override;

    IMPLEMENT_REFCOUNTING(CefPhysFsSchemeHandlerFactory);
};

#endif // USE_CEF