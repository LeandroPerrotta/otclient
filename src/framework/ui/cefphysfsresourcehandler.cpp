#include "cefphysfsresourcehandler.h"

#ifdef USE_CEF

#include <framework/core/resourcemanager.h>
#include <include/cef_scheme.h>
#include <unordered_map>
#include <algorithm>
#include <cstring>

CefPhysFsResourceHandler::CefPhysFsResourceHandler(const std::string& path)
    : m_offset(0) {
    if (g_resources.fileExists(path)) {
        m_data = g_resources.readFileContents(path);
        auto dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            m_mimeType = getMimeType(path.substr(dot + 1));
        } else {
            m_mimeType = "text/plain";
        }
    }
}

bool CefPhysFsResourceHandler::ProcessRequest(CefRefPtr<CefRequest> /*request*/, CefRefPtr<CefCallback> callback) {
    callback->Continue();
    return true;
}

void CefPhysFsResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& /*redirectUrl*/) {
    if (!m_data.empty()) {
        response->SetMimeType(m_mimeType);
        response->SetStatus(200);
        response_length = m_data.size();
    } else {
        response->SetStatus(404);
        response_length = 0;
    }
}

bool CefPhysFsResourceHandler::Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> /*callback*/) {
    bytes_read = 0;
    if (m_offset >= m_data.size())
        return false;

    int transfer = (bytes_to_read < static_cast<int>(m_data.size() - m_offset)) ? bytes_to_read : static_cast<int>(m_data.size() - m_offset);
    memcpy(data_out, m_data.data() + m_offset, transfer);
    m_offset += transfer;
    bytes_read = transfer;
    return true;
}

void CefPhysFsResourceHandler::Cancel() {}

std::string CefPhysFsResourceHandler::getMimeType(const std::string& ext) {
    static const std::unordered_map<std::string, std::string> mimes = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"js", "application/javascript"},
        {"css", "text/css"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"json", "application/json"},
        {"ico", "image/x-icon"}
    };
    auto it = mimes.find(ext);
    if (it != mimes.end())
        return it->second;
    return "text/plain";
}

static std::string resolvePathFromUrl(const std::string& url) {
    const std::string scheme = "otclient://";
    const std::string httpHost = "http://otclient";
    const std::string httpsHost = "https://otclient";

    std::string path;
    if (url.rfind(scheme, 0) == 0)
        path = url.substr(scheme.size());
    else if (url.rfind(httpHost, 0) == 0)
        path = url.substr(httpHost.size());
    else if (url.rfind(httpsHost, 0) == 0)
        path = url.substr(httpsHost.size());

    if (!path.empty() && path[0] != '/')
        path.insert(path.begin(), '/');
    return path;
}

CefRefPtr<CefResourceHandler> CefPhysFsResourceRequestHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request) {
    const std::string path = resolvePathFromUrl(request->GetURL());
    if (!path.empty())
        return new CefPhysFsResourceHandler(path);
    return nullptr;
}

CefRefPtr<CefResourceHandler> CefPhysFsSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& /*scheme_name*/,
    CefRefPtr<CefRequest> request) {
    const std::string path = resolvePathFromUrl(request->GetURL());
    if (!path.empty())
        return new CefPhysFsResourceHandler(path);
    return nullptr;
}
#endif // USE_CEF
