#include "cef_config.h"

#ifdef USE_CEF

#include "cef_helper.h"
#include <framework/stdext/format.h>

// Include platform-specific configurations
#ifdef _WIN32
#include "cef_confwin.h"
#else
#include "cef_conflinux.h"
#endif

namespace cef {

// ============================================================================
// Base CefConfig Implementation
// ============================================================================

void CefConfig::applyGenericSettings(CefSettings& settings) {
    settings.windowless_rendering_enabled = m_genericSettings.windowless_rendering_enabled;
    settings.multi_threaded_message_loop = m_genericSettings.multi_threaded_message_loop;
    settings.no_sandbox = m_genericSettings.no_sandbox;
    settings.persist_session_cookies = m_genericSettings.persist_session_cookies;
    settings.external_message_pump = m_genericSettings.external_message_pump;
}

void CefConfig::applyGenericCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    // Only for debbuging, avoid add switchs that may lead to break builds on Windows
    // due too much characters on the command parameters
    // command_line->AppendSwitch("disable-gpu-shader-disk-cache");
}

CefBrowserSettings CefConfig::createBrowserSettings() {
    CefBrowserSettings settings;
    applyBrowserSettings(settings);
    return settings;
}

void CefConfig::applyBrowserSettings(CefBrowserSettings& settings) {
    (void)settings;
}



// ============================================================================
// CefConfigFactory Implementation
// ============================================================================

std::unique_ptr<CefConfig> CefConfigFactory::createConfig() {
    return createConfig(getCurrentPlatform());
}

std::unique_ptr<CefConfig> CefConfigFactory::createConfig(const std::string& platform) {
    if (platform == "Windows") {
#ifdef _WIN32
        return std::make_unique<CefConfigWindows>();
#else
        logMessage("ConfigFactory", "Windows config requested but not compiled for Windows");
        return nullptr;
#endif
    } else if (platform == "Linux") {
#ifndef _WIN32
        return std::make_unique<CefConfigLinux>();
#else
        logMessage("ConfigFactory", "Linux config requested but not compiled for Linux");
        return nullptr;
#endif
    }
    
    logMessage("ConfigFactory", stdext::format("Unknown platform: %s", platform).c_str());
    return nullptr;
}

std::string CefConfigFactory::getCurrentPlatform() {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux";
#endif
}

} // namespace cef

#endif // USE_CEF