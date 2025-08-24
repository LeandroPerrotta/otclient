#pragma once

#ifdef USE_CEF

#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include <memory>
#include <string>

namespace cef {

// Base configuration class for CEF settings and command line flags
// Provides cross-platform defaults with platform-specific overrides
class CefConfig {
public:
    // Generic settings that work across all platforms
    struct GenericSettings {
        bool windowless_rendering_enabled = true;
        bool multi_threaded_message_loop = true;
        bool no_sandbox = true;
        bool persist_session_cookies = true;
        bool external_message_pump = false;
    };
    
    // Command line flags that work across platforms
    struct GenericCommandLineFlags {
        bool enable_gpu = true;
        bool enable_gpu_compositing = true;
        bool enable_gpu_rasterization = true;
        bool disable_software_rasterizer = true;
        bool enable_begin_frame_scheduling = true;
        bool disable_background_timer_throttling = true;
        bool disable_renderer_backgrounding = true;
        bool disable_gpu_sandbox = true; // Often needed for shared textures
        
        // Disable unnecessary features to reduce noise
        bool disable_background_networking = true;
        bool disable_sync = true;
        bool disable_background_mode = true;
        bool disable_component_extensions_with_background_pages = true;
        bool disable_default_apps = true;
        bool disable_extensions = true;
        bool no_service_autorun = true;
    };

    virtual ~CefConfig() = default;
    
    // Apply CEF settings (paths, cache, etc.)
    virtual void applySettings(CefSettings& settings) = 0;
    
    // Apply command line flags for performance and features
    virtual void applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) = 0;
    
    // Configure platform-specific paths
    virtual void configurePaths(CefSettings& settings) = 0;
    
    // Get platform name for debugging/logging
    virtual std::string getPlatformName() const = 0;
    
    // Future: Allow user customization (for Lua integration)
    virtual void setUserPreference(const std::string& key, const std::string& value) {}
    virtual std::string getUserPreference(const std::string& key) const { return ""; }

protected:
    GenericSettings m_genericSettings;
    GenericCommandLineFlags m_genericFlags;
    
    // Helper to apply generic settings
    void applyGenericSettings(CefSettings& settings);
    void applyGenericCommandLineFlags(CefRefPtr<CefCommandLine> command_line);
};

// Platform-specific configurations are defined in separate files:
// - cef_confwin.h/cpp for CefConfigWindows
// - cef_conflinux.h/cpp for CefConfigLinux  
// - Future: cef_confmac.h/cpp for CefConfigMac, etc.

// Factory for creating platform-specific configurations
class CefConfigFactory {
public:
    // Create configuration for the current platform
    static std::unique_ptr<CefConfig> createConfig();
    
    // Create configuration for a specific platform (for testing)
    static std::unique_ptr<CefConfig> createConfig(const std::string& platform);
    
    // Get current platform name
    static std::string getCurrentPlatform();
};

} // namespace cef

#endif // USE_CEF