#include "cef_confwin.h"

#ifdef USE_CEF
#ifdef _WIN32

#include "cef_helper.h"
#include <framework/stdext/format.h>
#include <framework/core/logger.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>

// Only include scheme handler in main process, not subprocess
#ifndef CEF_SUBPROCESS_BUILD
#include <cef/resources/cefphysfsresourcehandler.h>
#include "include/cef_scheme.h"
#endif

namespace cef {

CefConfigWindows::CefConfigWindows() {
#if !defined(OPENGL_ES) || OPENGL_ES != 2
    m_genericFlags.enable_gpu = false;
    m_genericFlags.enable_gpu_compositing = false;
    m_genericFlags.enable_gpu_rasterization = false;
    m_genericFlags.disable_software_rasterizer = false;
    m_genericFlags.disable_gpu_sandbox = false;
#endif
}

std::wstring CefConfigWindows::getExecutableDirectory() const {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return L".";
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

void CefConfigWindows::setupDllDirectories() const {
    const std::wstring exeDir = getExecutableDirectory();
    const std::wstring cefDir = exeDir + L"\\cef";
    
    // Log directory depth analysis
    size_t exeDirDepth = std::count(exeDir.begin(), exeDir.end(), L'\\');
    logMessage("Windows", stdext::format("Executable directory: %s", 
        std::string(exeDir.begin(), exeDir.end())).c_str());
    logMessage("Windows", stdext::format("Directory depth: %zu levels", exeDirDepth).c_str());
    
    // Verify CEF directory exists before adding it
    DWORD fileAttrib = GetFileAttributesW(cefDir.c_str());
    if (fileAttrib == INVALID_FILE_ATTRIBUTES || !(fileAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        logMessage("Windows", stdext::format("WARNING: CEF directory not found at %s", 
            std::string(cefDir.begin(), cefDir.end())).c_str());
        return;
    }
    
    // Configure DLL search paths to prioritize the CEF directory
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    
    // Add CEF directory to DLL search path
    DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(cefDir.c_str());
    if (cookie == NULL) {
        DWORD error = GetLastError();
        logMessage("Windows", stdext::format("WARNING: Failed to add CEF directory to DLL search path (Error: %lu)", error).c_str());
    } else {
        logMessage("Windows", stdext::format("CEF DLL directory added successfully: %s", 
            std::string(cefDir.begin(), cefDir.end())).c_str());
    }
}

void CefConfigWindows::configurePaths(CefSettings& settings) {
    setupDllDirectories();
    const std::wstring exeDir = getExecutableDirectory();
    
    // Always use Windows TEMP directory for cache to avoid path length issues
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring tempDir = tempPath;
    
    // Create unique temp directory for this process
    DWORD processId = GetCurrentProcessId();
    std::wstring tempCefDir = tempDir + L"otclient_cef_" + std::to_wstring(processId);
    
    // Configure paths
    std::wstring cefDir = exeDir + L"\\cef";  // DLLs must be in original location
    std::wstring localesDir = cefDir + L"\\locales";
    std::wstring cacheDir = tempCefDir + L"\\cache";  // Cache always in TEMP
    std::wstring subprocessPath = cefDir + L"\\otclient_cef_subproc.exe";
    
    // Create temp cache directory
    CreateDirectoryW(tempCefDir.c_str(), nullptr);
    CreateDirectoryW(cacheDir.c_str(), nullptr);
    
    logMessage("Windows", stdext::format("Using TEMP cache directory: %s", 
        std::string(cacheDir.begin(), cacheDir.end())).c_str());

    // Verify CEF directory exists and contains required files
    std::wstring libcefPath = cefDir + L"\\libcef.dll";
    DWORD fileAttrib = GetFileAttributesW(libcefPath.c_str());
    if (fileAttrib == INVALID_FILE_ATTRIBUTES) {
        g_logger.error(stdext::format("CEF: ERROR: libcef.dll not found at %s", 
            std::string(libcefPath.begin(), libcefPath.end())));
        g_logger.info("CEF: Make sure to copy the CEF runtime files to the ./cef/ directory");
        return;
    }

    CefString(&settings.resources_dir_path) = cefDir;
    CefString(&settings.locales_dir_path) = localesDir;
    CefString(&settings.cache_path) = cacheDir;
    CefString(&settings.root_cache_path) = cacheDir;
    CefString(&settings.browser_subprocess_path) = subprocessPath;

    // Detailed logging for debugging path depth issues
    g_logger.info(stdext::format("CEF: CEF directory: %s", std::string(cefDir.begin(), cefDir.end())));
    g_logger.info(stdext::format("CEF: CEF subprocess path: %s", std::string(subprocessPath.begin(), subprocessPath.end())));
    g_logger.info(stdext::format("CEF: CEF cache path: %s", std::string(cacheDir.begin(), cacheDir.end())));
    g_logger.info(stdext::format("CEF: CEF locales path: %s", std::string(localesDir.begin(), localesDir.end())));
    
    // Check path lengths - this is critical for CEF functionality
    std::string exeDirStr = std::string(exeDir.begin(), exeDir.end());
    std::string cefDirStr = std::string(cefDir.begin(), cefDir.end());
    std::string cacheDirStr = std::string(cacheDir.begin(), cacheDir.end());
    std::string subprocessPathStr = std::string(subprocessPath.begin(), subprocessPath.end());
    
    g_logger.info(stdext::format("CEF: === PATH LENGTH ANALYSIS ==="));
    g_logger.info(stdext::format("CEF: Executable dir: %s (%zu chars)", exeDirStr.c_str(), exeDirStr.length()));
    g_logger.info(stdext::format("CEF: CEF dir: %s (%zu chars)", cefDirStr.c_str(), cefDirStr.length()));
    g_logger.info(stdext::format("CEF: Cache dir: %s (%zu chars)", cacheDirStr.c_str(), cacheDirStr.length()));
    g_logger.info(stdext::format("CEF: Subprocess path: %s (%zu chars)", subprocessPathStr.c_str(), subprocessPathStr.length()));
    
    // Critical thresholds based on Windows limitations
    if (exeDirStr.length() > 100) {
        g_logger.info("CEF: WARNING: Executable directory path > 100 chars - CEF GPU process may fail!");
    }
    if (subprocessPathStr.length() > 200) {
        g_logger.info("CEF: WARNING: Subprocess path > 200 chars - CEF may fail to start subprocess!");
    }
    if (cacheDirStr.length() > 180) {
        g_logger.info("CEF: WARNING: Cache directory path > 180 chars - CEF cache operations may fail!");
    }
    
    g_logger.info(stdext::format("CEF: === END PATH ANALYSIS ==="));
    
    g_logger.info("CEF: CEF configured for portable operation");
}

void CefConfigWindows::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void CefConfigWindows::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    applyGenericCommandLineFlags(command_line);
    
    // Check if we're in a long path and apply specific workarounds
    std::string exeDir = std::string(getExecutableDirectory().begin(), getExecutableDirectory().end());
    
    // Log exact path length for debugging
    g_logger.info("=== PATH LENGTH DEBUG ===");
    g_logger.info(stdext::format("Executable directory: %s", exeDir.c_str()));
    g_logger.info(stdext::format("Path length: %zu characters", exeDir.length()));
    
    // Count directory depth
    size_t depth = std::count(exeDir.begin(), exeDir.end(), '\\');
    g_logger.info(stdext::format("Directory depth: %zu levels", depth));
    
    // The threshold appears to be much lower than MAX_PATH
    // Based on user testing: works at ~30 chars, fails at ~50+ chars
    // Even with compressed packages, CEF still fails in long paths!
    bool isLongPath = exeDir.length() > 30; // VERY conservative threshold
    
    // Always add flags to force temporary files to Windows TEMP directory
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::string tempDir = std::string(tempPath, tempPath + wcslen(tempPath));
    
    // Force CEF to use TEMP directory for all temporary files
    command_line->AppendSwitchWithValue("disk-cache-dir", tempDir + "otclient_cef_disk_cache");
    command_line->AppendSwitchWithValue("user-data-dir", tempDir + "otclient_cef_user_data");
    command_line->AppendSwitch("disable-dev-shm-usage"); // Don't use /dev/shm (Linux) or equivalent
    
    g_logger.info(stdext::format("Forcing all temp files to: %s", tempDir.c_str()));
    
    if (isLongPath) {
        g_logger.info(stdext::format("Long path detected (%zu chars), applying CEF workarounds", exeDir.length()));
        
        // Critical flags for long paths - based on Chromium bug reports
        command_line->AppendSwitch("disable-gpu-process-crash-limit");
        command_line->AppendSwitch("disable-gpu-process-prelaunch");  
        command_line->AppendSwitch("disable-gpu-early-init");
        command_line->AppendSwitch("no-zygote");
        
        // For paths > 30 chars, immediately disable GPU process
        g_logger.info("Long path detected - disabling GPU process to avoid named pipe issues");
        command_line->AppendSwitch("disable-gpu");
        command_line->AppendSwitch("disable-software-rasterizer");
        
        // For paths > 50 chars, force single process mode immediately
        if (exeDir.length() > 50) {
            g_logger.info("Very long path detected, enabling single-process mode");
            command_line->AppendSwitch("single-process");
        }
        
        g_logger.info("Applied long path workaround flags");
    } else {
        g_logger.info(stdext::format("Normal path length (%zu chars), using standard flags", exeDir.length()));
    }
    
    // Always add these for debugging
    command_line->AppendSwitch("enable-logging");
    command_line->AppendSwitchWithValue("log-level", "0");
    
    g_logger.info(stdext::format("CEF: Command line flags: %s",
        command_line->GetCommandLineString().ToString()));
}

CefMainArgs CefConfigWindows::createMainArgs(int argc, const char* argv[]) {
    return CefMainArgs(GetModuleHandle(nullptr));
}

bool CefConfigWindows::handleSubprocessExecution(const CefMainArgs& args, CefRefPtr<CefApp> app) {
    setupDllDirectories();
    // Early-subprocess exit (the main executable should never be used as subprocess
    // when browser_subprocess_path is defined, but call CefExecuteProcess for completeness)
    const int code = CefExecuteProcess(args, nullptr, nullptr);
    g_logger.info(stdext::format("CEF: CefExecuteProcess returned code: %d", code).c_str());
    if (code >= 0) {
        std::exit(code);
        return true; // Never reached
    }
    return false;
}

void CefConfigWindows::registerSchemeHandlers() {
#ifndef CEF_SUBPROCESS_BUILD
    // Scheme handlers are only needed in the main process, not in subprocess
    CefRegisterSchemeHandlerFactory("otclient", "", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("http", "otclient", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("https", "otclient", new CefPhysFsSchemeHandlerFactory);
    g_logger.info("CEF: Scheme handlers registered");
#else
    g_logger.info("CEF: Skipping scheme handler registration in subprocess");
#endif
}

bool CefConfigWindows::shouldUseSharedTexture() const {
#if defined(OPENGL_ES) && OPENGL_ES == 2
    return true;
#else
    return false;
#endif
}

} // namespace cef

#endif // _WIN32
#endif // USE_CEF