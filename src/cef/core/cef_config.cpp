#include "cef_config.h"

#ifdef USE_CEF

#include "cef_helper.h"
#include <framework/core/logger.h>
#include <framework/core/resourcemanager.h>
#include <framework/stdext/format.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <climits>
#include <cstdlib>
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
    if (m_genericFlags.enable_gpu)
        command_line->AppendSwitch("enable-gpu");
    if (m_genericFlags.enable_gpu_compositing)
        command_line->AppendSwitch("enable-gpu-compositing");
    if (m_genericFlags.enable_gpu_rasterization)
        command_line->AppendSwitch("enable-gpu-rasterization");
    if (m_genericFlags.disable_software_rasterizer)
        command_line->AppendSwitch("disable-software-rasterizer");
    if (m_genericFlags.disable_gpu_sandbox)
        command_line->AppendSwitch("disable-gpu-sandbox");
    
    if (m_genericFlags.enable_begin_frame_scheduling)
        command_line->AppendSwitch("enable-begin-frame-scheduling");
    if (m_genericFlags.disable_background_timer_throttling)
        command_line->AppendSwitch("disable-background-timer-throttling");
    if (m_genericFlags.disable_renderer_backgrounding)
        command_line->AppendSwitch("disable-renderer-backgrounding");
    
    // Disable unnecessary features
    if (m_genericFlags.disable_background_networking)
        command_line->AppendSwitch("disable-background-networking");
    if (m_genericFlags.disable_sync)
        command_line->AppendSwitch("disable-sync");
    if (m_genericFlags.disable_background_mode)
        command_line->AppendSwitch("disable-background-mode");
    if (m_genericFlags.disable_component_extensions_with_background_pages)
        command_line->AppendSwitch("disable-component-extensions-with-background-pages");
    if (m_genericFlags.disable_default_apps)
        command_line->AppendSwitch("disable-default-apps");
    if (m_genericFlags.disable_extensions)
        command_line->AppendSwitch("disable-extensions");
    if (m_genericFlags.no_service_autorun)
        command_line->AppendSwitch("no-service-autorun");
    
    // Disable features that cause noise in logs
    command_line->AppendSwitch("disable-features=PushMessaging,BackgroundSync,GCM");
}

// ============================================================================
// Windows CEF Configuration
// ============================================================================

#ifdef _WIN32

std::wstring WindowsCefConfig::getExecutableDirectory() const {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return L".";
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

void WindowsCefConfig::configurePaths(CefSettings& settings) {
    const std::wstring exeDir = getExecutableDirectory();
    const std::wstring cefDir = exeDir + L"\\cef";
    const std::wstring localesDir = cefDir + L"\\locales";
    const std::wstring cacheDir = cefDir + L"\\cache";
    const std::wstring subprocessPath = cefDir + L"\\otclient_cef_subproc.exe";

    // Setup libcef.dll delay-loaded
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    AddDllDirectory(cefDir.c_str());

    CefString(&settings.resources_dir_path) = cefDir;
    CefString(&settings.locales_dir_path) = localesDir;
    CefString(&settings.cache_path) = cacheDir;
    CefString(&settings.root_cache_path) = cacheDir;
    CefString(&settings.browser_subprocess_path) = subprocessPath;
    
    logMessage("Windows", stdext::format("CEF directory: %s", std::string(cefDir.begin(), cefDir.end())).c_str());
}

void WindowsCefConfig::configureAngle(CefRefPtr<CefCommandLine> command_line) {
#if defined(OPENGL_ES) && OPENGL_ES == 2
    command_line->AppendSwitch("angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    command_line->AppendSwitch("shared-texture-enabled");
    command_line->AppendSwitch("disable-gpu-watchdog"); // Prevent GPU process timeout
    
    logMessage("Windows", "ANGLE D3D11 configuration applied");
#endif
}

void WindowsCefConfig::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void WindowsCefConfig::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    configureAngle(command_line);
    applyGenericCommandLineFlags(command_line);
    
    logMessage("Windows", stdext::format("Command line flags: %s", 
        command_line->GetCommandLineString().ToString()).c_str());
}

#endif // _WIN32

// ============================================================================
// Linux CEF Configuration
// ============================================================================

#ifndef _WIN32

std::string LinuxCefConfig::findCefDirectory() const {
    std::string work_dir;
    
    // Try to get work directory, but use current directory as fallback
    try {
        work_dir = g_resources.getWorkDir();
    } catch (...) {
        work_dir = ".";
    }
    
    // If work_dir is empty or invalid, use current directory
    if (work_dir.empty() || work_dir == ".") {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            work_dir = cwd;
        } else {
            work_dir = ".";
        }
    }
    
    logMessage("Linux", stdext::format("Work directory: %s", work_dir).c_str());

    // Look for CEF in the local runtime directory (where setup_cef.sh copies everything)
    std::vector<std::string> search_paths = {
        work_dir + "/cef",                      // Local runtime directory
        "./cef",                                // Current directory fallback
        work_dir + "/cef_binary_*",             // Legacy locations (fallback)
        work_dir + "/../cef_binary_*",
        "./cef_binary_*",
        "/usr/local/cef",
        "/opt/cef"
    };
    
    // Add environment variable if set
    const char* env_cef_root = getenv("CEF_ROOT");
    if (env_cef_root) {
        search_paths.insert(search_paths.begin(), env_cef_root);
    }
    
    // Find CEF directory
    for (const auto& path : search_paths) {
        if (path.find("*") != std::string::npos) {
            // Handle glob pattern
            std::string pattern = path;
            size_t pos = pattern.find("*");
            std::string prefix = pattern.substr(0, pos);
            std::string suffix = pattern.substr(pos + 1);
            
            // Simple glob implementation
            DIR* dir = opendir(prefix.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name.find("cef_binary_") == 0 && name.find(suffix) != std::string::npos) {
                        std::string full_path = prefix + name;
                        if (access((full_path + "/include/cef_app.h").c_str(), F_OK) == 0) {
                            closedir(dir);
                            return full_path;
                        }
                    }
                }
                closedir(dir);
            }
        } else {
            // Direct path - check for runtime directory first
            if (path.find("/cef") != std::string::npos) {
                // This is the runtime directory, check for libcef.so
                if (access((path + "/libcef.so").c_str(), F_OK) == 0) {
                    return path;
                }
            } else {
                // Check for include directory
                if (access((path + "/include/cef_app.h").c_str(), F_OK) == 0) {
                    return path;
                }
            }
        }
    }
    
    return ""; // Not found
}

void LinuxCefConfig::configurePaths(CefSettings& settings) {
    std::string cef_root = findCefDirectory();
    
    if (cef_root.empty()) {
        logMessage("Linux", "CEF not found. Please run ./setup_cef.sh first");
        return;
    }
    
    // Determine if this is the runtime directory or the full CEF directory
    std::string resourcesPath;
    if (access((cef_root + "/libcef.so").c_str(), F_OK) == 0) {
        // This is the runtime directory
        resourcesPath = cef_root + "/";
    } else {
        // This is the full CEF directory
        resourcesPath = cef_root + "/Release/";
    }
    
    std::string localesPath = resourcesPath + "locales";
    std::string cache_path = cef_root + "/cache";
    std::string subprocess_path = cef_root + "/otclient_cef_subproc";
    
    logMessage("Linux", stdext::format("CEF found at: %s", cef_root).c_str());
    logMessage("Linux", stdext::format("CEF resources path: %s", resourcesPath).c_str());
    logMessage("Linux", stdext::format("CEF locales path: %s", localesPath).c_str());
    
    // Check if icudtl.dat exists
    std::string icuPath = resourcesPath + "icudtl.dat";
    if (access(icuPath.c_str(), F_OK) == 0) {
        logMessage("Linux", stdext::format("icudtl.dat found at %s", icuPath).c_str());
    } else {
        logMessage("Linux", stdext::format("icudtl.dat NOT found at %s", icuPath).c_str());
        return;
    }
    
    // Convert relative paths to absolute paths
    char abs_cache_path[PATH_MAX];
    char abs_resources_path[PATH_MAX];
    char abs_locales_path[PATH_MAX];
    
    realpath(cache_path.c_str(), abs_cache_path);
    realpath(resourcesPath.c_str(), abs_resources_path);
    realpath(localesPath.c_str(), abs_locales_path);
    
    logMessage("Linux", stdext::format("CEF cache path: %s", abs_cache_path).c_str());

    CefString(&settings.cache_path) = abs_cache_path;
    CefString(&settings.root_cache_path) = abs_cache_path;
    CefString(&settings.resources_dir_path) = abs_resources_path;
    CefString(&settings.locales_dir_path) = abs_locales_path;
    CefString(&settings.browser_subprocess_path) = subprocess_path;
}

void LinuxCefConfig::configureX11(CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitchWithValue("ozone-platform", "x11");
    logMessage("Linux", "X11 platform configuration applied");
}

void LinuxCefConfig::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void LinuxCefConfig::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    configureX11(command_line);
    applyGenericCommandLineFlags(command_line);
    
    logMessage("Linux", stdext::format("Command line flags: %s", 
        command_line->GetCommandLineString().ToString()).c_str());
}

#endif // !_WIN32

// ============================================================================
// CefConfigFactory Implementation
// ============================================================================

std::unique_ptr<CefConfig> CefConfigFactory::createConfig() {
    return createConfig(getCurrentPlatform());
}

std::unique_ptr<CefConfig> CefConfigFactory::createConfig(const std::string& platform) {
    if (platform == "Windows") {
#ifdef _WIN32
        return std::make_unique<WindowsCefConfig>();
#else
        logMessage("ConfigFactory", "Windows config requested but not compiled for Windows");
        return nullptr;
#endif
    } else if (platform == "Linux") {
#ifndef _WIN32
        return std::make_unique<LinuxCefConfig>();
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