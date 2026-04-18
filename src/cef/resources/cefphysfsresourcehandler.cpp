#include "cefphysfsresourcehandler.h"

#ifdef USE_CEF

#include <client/thingtypemanager.h>
#include <client/const.h>
#include <framework/core/resourcemanager.h>
#include <framework/graphics/image.h>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
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

CefPhysFsResourceHandler::CefPhysFsResourceHandler(std::string data, std::string mimeType)
    : m_data(std::move(data)), m_mimeType(std::move(mimeType)), m_offset(0) {
}

namespace {
static std::mutex g_creaturePngMutex;
static std::unordered_map<uint16_t, std::string> g_creaturePngCache;
static constexpr size_t CREATURE_PNG_CACHE_MAX = 256;

static bool parseCreatureTypePath(const std::string& path, uint16_t& outLookType)
{
    static const char kPrefix[] = "/creature/type/";
    if(path.compare(0, sizeof(kPrefix) - 1, kPrefix) != 0)
        return false;
    const char* start = path.c_str() + (sizeof(kPrefix) - 1);
    if(*start == '\0')
        return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(start, &end, 10);
    if(end == start || v == 0UL || v > 65535UL)
        return false;
    if(*end != '\0' && *end != '/')
        return false;
    outLookType = static_cast<uint16_t>(v);
    return true;
}

static std::string buildCreatureTypePng(uint16_t lookType)
{
    if(!g_things.isDatLoaded())
        return {};
    if(!g_things.isValidDatId(lookType, ThingCategoryCreature))
        return {};
    const ThingTypePtr& tt = g_things.getThingType(lookType, ThingCategoryCreature);
    if(!tt || tt->isNull())
        return {};
    ImagePtr img = tt->toImageFrame(Otc::South, 0, 0, 0);
    if(!img)
        return {};
    return img->encodePNG();
}

static CefRefPtr<CefResourceHandler> tryCreateCreatureTypeHandler(const std::string& path)
{
    uint16_t lookType = 0;
    if(!parseCreatureTypePath(path, lookType))
        return nullptr;

    std::string png;
    {
        std::lock_guard<std::mutex> lock(g_creaturePngMutex);
        auto it = g_creaturePngCache.find(lookType);
        if(it != g_creaturePngCache.end()) {
            png = it->second;
        } else {
            png = buildCreatureTypePng(lookType);
            if(!png.empty()) {
                if(g_creaturePngCache.size() >= CREATURE_PNG_CACHE_MAX)
                    g_creaturePngCache.clear();
                g_creaturePngCache[lookType] = png;
            }
        }
    }

    if(!png.empty())
        return new CefPhysFsResourceHandler(std::move(png), "image/png");
    return new CefPhysFsResourceHandler(std::string(), "image/png");
}
} // namespace

bool CefPhysFsResourceHandler::ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) {
    m_isOptionsRequest = false;
    m_status = 200;
    if (request) {
        std::string method = request->GetMethod();
        if (!method.empty()) {
            std::transform(method.begin(), method.end(), method.begin(), ::toupper);
            if (method == "OPTIONS") {
                m_isOptionsRequest = true;
                m_status = 204; // No Content for preflight
            }
        }
    }
    callback->Continue();
    return true;
}

void CefPhysFsResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& response_length, CefString& /*redirectUrl*/) {
    if (!m_data.empty() && !m_isOptionsRequest) {
        response->SetMimeType(m_mimeType);
        response->SetStatus(m_status);
        response_length = m_data.size();
    } else {
        response->SetStatus(m_isOptionsRequest ? m_status : 404);
        response_length = 0;
    }

    // Add permissive CORS headers so fetch/XHR works from the webview
    CefResponse::HeaderMap headers;
    response->GetHeaderMap(headers);
    headers.insert({"Access-Control-Allow-Origin", "*"});
    headers.insert({"Access-Control-Allow-Methods", "GET, OPTIONS"});
    headers.insert({"Access-Control-Allow-Headers", "Content-Type"});
    headers.insert({"Access-Control-Expose-Headers", "Content-Type"});
    response->SetHeaderMap(headers);
}

bool CefPhysFsResourceHandler::Read(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefResourceReadCallback> /*callback*/) {
    bytes_read = 0;
    if (m_isOptionsRequest || m_offset >= m_data.size())
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
    const auto q = path.find_first_of("?#");
    if(q != std::string::npos)
        path.resize(q);
    return path;
}

CefRefPtr<CefResourceHandler> CefPhysFsResourceRequestHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    CefRefPtr<CefRequest> request) {
    const std::string path = resolvePathFromUrl(request->GetURL());
    if (!path.empty()) {
        if(CefRefPtr<CefResourceHandler> h = tryCreateCreatureTypeHandler(path))
            return h;
        return new CefPhysFsResourceHandler(path);
    }
    return nullptr;
}

CefRefPtr<CefResourceHandler> CefPhysFsSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> /*browser*/,
    CefRefPtr<CefFrame> /*frame*/,
    const CefString& /*scheme_name*/,
    CefRefPtr<CefRequest> request) {
    const std::string path = resolvePathFromUrl(request->GetURL());
    if (!path.empty()) {
        if(CefRefPtr<CefResourceHandler> h = tryCreateCreatureTypeHandler(path))
            return h;
        return new CefPhysFsResourceHandler(path);
    }
    return nullptr;
}
#endif // USE_CEF
