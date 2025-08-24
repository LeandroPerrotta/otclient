#include "cef_conflinux.h"

#ifdef USE_CEF
#ifndef _WIN32

#include "cef_helper.h"
#include <framework/stdext/format.h>

#include <unistd.h>
#include <dirent.h>
#include <climits>
#include <cstdlib>
#include <vector>

namespace cef {

std::string CefConfigLinux::findCefDirectory() const {
    std::string work_dir;
    
    // Get current working directory (works in both main process and subprocess)
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        work_dir = cwd;
    } else {
        work_dir = ".";
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

void CefConfigLinux::configurePaths(CefSettings& settings) {
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

void CefConfigLinux::configureX11(CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitchWithValue("ozone-platform", "x11");
    logMessage("Linux", "X11 platform configuration applied");
}

void CefConfigLinux::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void CefConfigLinux::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    configureX11(command_line);
    applyGenericCommandLineFlags(command_line);
    
    logMessage("Linux", stdext::format("Command line flags: %s", 
        command_line->GetCommandLineString().ToString()).c_str());
}

} // namespace cef

#endif // !_WIN32
#endif // USE_CEF