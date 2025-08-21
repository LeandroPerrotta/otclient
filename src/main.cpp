/*
 * Copyright (c) 2010-2020 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <framework/core/application.h>
#include <framework/core/resourcemanager.h>
#include <framework/luaengine/luainterface.h>
#include <client/client.h>

#ifdef USE_CEF
#include <framework/ui/uicefwebview.h>
#include <framework/ui/cefphysfsresourcehandler.h>
#endif

#ifdef USE_CEF
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>
#include <io.h>
#include <direct.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif
#include <limits.h>
#include <vector>
#include <string>
#include <ctime>
#endif

#ifdef USE_CEF
// Global CEF state
bool g_cefInitialized = false;

// Raw logger for early CEF initialization (no dependencies)
void rawLogger(const char* message) {
    FILE* log_file = fopen("cef.log", "a");
    if (log_file) {
        // Get current time
        time_t now = time(0);
        char* timestr = ctime(&now);
        // Remove newline from timestr
        if (timestr && strlen(timestr) > 0) {
            timestr[strlen(timestr) - 1] = '\0';
        }
        
        fprintf(log_file, "[%s] %s\n", timestr ? timestr : "unknown", message);
        fflush(log_file);
        fclose(log_file);
    }
    
    // Also print to console
    printf("[CEF] %s\n", message);
    fflush(stdout);
}

// Minimal CEF app used by the browser process to register custom schemes
class OTClientBrowserApp : public CefApp {
public:
    OTClientBrowserApp() {}

    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
        registrar->AddCustomScheme("otclient",
                                   CEF_SCHEME_OPTION_STANDARD |
                                   CEF_SCHEME_OPTION_LOCAL |
                                   CEF_SCHEME_OPTION_DISPLAY_ISOLATED);
    }

    // Enable GPU acceleration when building on Windows with OpenGL ES 2.0
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override {
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
        // Ensure GPU accelerated rendering via ANGLE/DirectX.
        // These switches enable GPU compositing and rasterization.
        command_line->AppendSwitch("enable-gpu");
        command_line->AppendSwitch("enable-gpu-rasterization");
        command_line->AppendSwitch("enable-zero-copy");
        // Try OpenGL backend instead of D3D11 to avoid passthrough issues
        command_line->AppendSwitchWithValue("use-angle", "gl");
        // Disable passthrough to avoid ANGLE conflicts
        command_line->AppendSwitch("disable-gpu-passthrough");
        
        // Additional switches for OnAcceleratedPaint support
        command_line->AppendSwitch("enable-gpu-compositing");
        command_line->AppendSwitch("enable-accelerated-2d-canvas");
        command_line->AppendSwitch("disable-gpu-sandbox");
        // Force GPU process and disable software fallback
        command_line->AppendSwitch("disable-software-rasterizer");
        command_line->AppendSwitch("ignore-gpu-blacklist");
#endif
        
        // For all platforms - try to enable GPU process (but don't force it)
        if (process_type.empty()) { // Browser process
            // Only enable GPU if we're on Windows with OpenGL ES
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
            command_line->AppendSwitch("enable-gpu");
            command_line->AppendSwitch("enable-gpu-compositing");
#endif
        }
    }

    IMPLEMENT_REFCOUNTING(OTClientBrowserApp);
};

// Global CEF initialization function (simplified)
bool InitializeCEF(int argc, const char* argv[]) {

#ifdef _WIN32
    // 1) Descobrir caminhos absolutos
    auto getExeDirW = []() -> std::wstring {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (!n || n >= MAX_PATH) return L".";
        std::wstring p(buf, n);
        size_t pos = p.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
    };

    const std::wstring exeDir = getExeDirW();
    const std::wstring cefDir = exeDir + L"\\cef";
    const std::wstring localesDir = cefDir + L"\\locales";
    const std::wstring cacheDir = cefDir + L"\\cache";
    const std::wstring subprocessPath = cefDir + L"\\otclient_cef_subproc.exe";


    // 1) Setup libcef.dll delay-loaded
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    AddDllDirectory(cefDir.c_str());

    // 2) Early-subprocess exit (the main executable should never be used as subprocess
    // when browser_subprocess_path is defined, but call CefExecuteProcess for
    // completeness)
    CefMainArgs main_args(GetModuleHandle(nullptr));
    {
        const int code = CefExecuteProcess(main_args, nullptr, nullptr);
        rawLogger(("CefExecuteProcess returned code: " + std::to_string(code)).c_str());
        if (code >= 0)
            std::exit(code);
    }         

    // 3) Configuração de CEF
    CefSettings settings;
    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = true;
    settings.multi_threaded_message_loop = false;

    CefString(&settings.resources_dir_path)      = cefDir;      // .\cef
    CefString(&settings.locales_dir_path)        = localesDir;  // .\cef\locales
    CefString(&settings.cache_path)              = cacheDir;    // .\cef\cache
    CefString(&settings.user_data_path)          = cacheDir;    // reusa cache p/ prefs/cookies
    CefString(&settings.browser_subprocess_path) = subprocessPath; // subprocesso dedicado
    settings.persist_session_cookies             = true;
    settings.persist_user_preferences            = true;

    // 4) Inicializa
    CefRefPtr<CefApp> app = new OTClientBrowserApp();
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        rawLogger("FAILED to initialize CEF!");
        return false;
    }

    // 5) Registros opcionais (schemes, handlers, etc.)
    CefRegisterSchemeHandlerFactory("otclient", "", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("http",  "otclient", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("https", "otclient", new CefPhysFsSchemeHandlerFactory);
    g_cefInitialized = true;
    rawLogger("CEF initialized and scheme handlers registered");

    return true;
#else
    CefMainArgs main_args(argc, const_cast<char**>(argv));

    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        std::exit(exit_code);
    }

    // Parse command line
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    command_line->InitFromArgv(argc, argv);
    
    // Disable CEF auto-restart and subprocess management
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-background-timer-throttling");
    command_line->AppendSwitch("disable-renderer-backgrounding");
    command_line->AppendSwitch("disable-backgrounding-occluded-windows");
    command_line->AppendSwitch("disable-features=TranslateUI");
    command_line->AppendSwitch("disable-ipc-flooding-protection");
    


    // Configure CEF settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = true;
    settings.multi_threaded_message_loop = false;
    
    // Enable GPU acceleration for OnAcceleratedPaint
    settings.chrome_runtime = false; // OSR requires legacy runtime
    settings.external_message_pump = false;
    
    // Find CEF automatically (same logic as CMake)
    std::string cef_root;
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
    
    g_logger.info(stdext::format("OTClient: Work directory: %s", work_dir));

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
                            cef_root = full_path;
                            break;
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
                    cef_root = path;
                    break;
                }
            } else {
                // Check for include directory
                if (access((path + "/include/cef_app.h").c_str(), F_OK) == 0) {
                    cef_root = path;
                    break;
                }
            }
        }
    }
    
    if (cef_root.empty()) {
        g_logger.error("CEF not found. Please run ./setup_cef.sh first");
        return false;
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
    
    g_logger.info(stdext::format("OTClient: CEF found at: %s", cef_root));
    g_logger.info(stdext::format("OTClient: CEF resources path: %s", resourcesPath));
    g_logger.info(stdext::format("OTClient: CEF locales path: %s", localesPath));
    
    // Check if icudtl.dat exists
    std::string icuPath = resourcesPath + "icudtl.dat";
    if (access(icuPath.c_str(), F_OK) == 0) {
        g_logger.info(stdext::format("OTClient: icudtl.dat found at %s", icuPath));
    } else {
        g_logger.error(stdext::format("OTClient: icudtl.dat NOT found at %s", icuPath));
        return false;
    }

    // Configure cache path in CEF directory
    std::string cache_path = cef_root + "/cache";
    
    // Convert relative paths to absolute paths
    char abs_cache_path[PATH_MAX];
    char abs_resources_path[PATH_MAX];
    char abs_locales_path[PATH_MAX];
    
    realpath(cache_path.c_str(), abs_cache_path);
    realpath(resourcesPath.c_str(), abs_resources_path);
    realpath(localesPath.c_str(), abs_locales_path);
    
    g_logger.info(stdext::format("OTClient: CEF cache path: %s", abs_cache_path));
    
    // Configure CEF settings for data persistence
    CefString(&settings.cache_path) = abs_cache_path;
    CefString(&settings.user_data_path) = abs_cache_path;
    settings.persist_session_cookies = true;
    settings.persist_user_preferences = true;

    CefString(&settings.resources_dir_path) = abs_resources_path;
    CefString(&settings.locales_dir_path) = abs_locales_path;

    // Define subprocess path (same directory as main executable)
    std::string subprocess_path = cef_root + "/otclient_cef_subproc";
    CefString(&settings.browser_subprocess_path) = subprocess_path;

    g_logger.info("OTClient: Initializing CEF...");
    CefRefPtr<CefApp> app = new OTClientBrowserApp();
    bool result = CefInitialize(main_args, settings, app, nullptr);
    if (result) {
        CefRegisterSchemeHandlerFactory("otclient", "", new CefPhysFsSchemeHandlerFactory);
        CefRegisterSchemeHandlerFactory("http", "otclient", new CefPhysFsSchemeHandlerFactory);
        CefRegisterSchemeHandlerFactory("https", "otclient", new CefPhysFsSchemeHandlerFactory);
        g_cefInitialized = true;
        g_logger.info("OTClient: CEF initialized successfully");
    } else {
        g_logger.error("OTClient: Failed to initialize CEF");
    }

    return result;
#endif    
}

void ShutdownCEF() {
    if (g_cefInitialized) {
        rawLogger("Starting CEF shutdown...");

        // Close all active WebViews first
        UICEFWebView::closeAllWebViews();

        CefDoMessageLoopWork();

        rawLogger("All webviews closed... Shutting down CEF");

        // Shutdown CEF
        CefShutdown();
        g_cefInitialized = false;
        rawLogger("CEF shutdown completed");
    }
}
#else
// Stub when CEF is not enabled
bool g_cefInitialized = false;
bool InitializeCEF(int argc, const char* argv[]) { return true; }
void ShutdownCEF() {}
#endif

int main(int argc, const char* argv[])
{
#ifdef USE_CEF   
    // Initialize CEF FIRST, before anything else
    if (!InitializeCEF(argc, argv)) {
        rawLogger("InitializeCEF returned FALSE - exiting");
        return 1;
    }
    
    rawLogger("InitializeCEF returned TRUE - continuing with application startup");    
#endif

    std::vector<std::string> args(argv, argv + argc);

    // setup application name and version
    g_app.setName("OTClient");
    g_app.setCompactName("otclient");
    g_app.setVersion(VERSION);

    // initialize application framework and otclient
    g_app.init(args);
    g_client.init(args);

    // find script init.lua and run it
    if(!g_resources.discoverWorkDir("init.lua"))
        g_logger.fatal("Unable to find work directory, the application cannot be initialized.");

    if(!g_lua.safeRunScript("init.lua"))
        g_logger.fatal("Unable to run script init.lua!");

    // the run application main loop
    g_app.run();

#ifdef USE_CEF
    // Shutdown CEF BEFORE terminating the application
    ShutdownCEF();
#endif

    // unload modules
    g_app.deinit();

    // terminate everything and free memory
    g_client.terminate();
    g_app.terminate();

    return 0;
}
