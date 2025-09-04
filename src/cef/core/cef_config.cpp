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
    // Disable unnecessary features
    if (m_genericFlags.disable_background_networking)
        command_line->AppendSwitch("disable-background-networking");
    if (m_genericFlags.disable_default_apps)
        command_line->AppendSwitch("disable-default-apps");
    if (m_genericFlags.disable_extensions)
        command_line->AppendSwitch("disable-extensions");
    if (m_genericFlags.no_service_autorun)
        command_line->AppendSwitch("no-service-autorun");
    
    // Disable features that cause noise in logs
    command_line->AppendSwitch("disable-features=PushMessaging,BackgroundSync,GCM");
}

std::string CefConfig::getUserPreference(const std::string& key) const {
    // Check for environment variables first (for easy testing)
    if (key == "test_intel_graphics") {
        const char* env_val = getenv("CEF_TEST_INTEL_GRAPHICS");
        if (env_val) {
            return std::string(env_val);
        }
    }
    
    if (key == "allow_intel_graphics_override") {
        const char* env_val = getenv("CEF_ALLOW_INTEL_OVERRIDE");
        if (env_val) {
            return std::string(env_val);
        }
    }
    
    // Check internal preferences map
    auto it = m_preferences.find(key);
    if (it != m_preferences.end()) {
        return it->second;
    }
    
    return "";
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