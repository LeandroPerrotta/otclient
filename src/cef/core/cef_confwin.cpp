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
    
    // Configure paths
    std::wstring cefDir = exeDir + L"\\cef";  // DLLs must be in original location
    std::wstring cacheDir = L"C:\\cef_temp";  // Fixed cache directory
    std::wstring subprocessPath = cefDir + L"\\sp.exe";
    
    // Create cache directory
    CreateDirectoryW(cacheDir.c_str(), nullptr);
    
    logMessage("Windows", "Using fixed cache directory: C:\\cef_temp");

    // Verify CEF directory exists and contains required files
    std::wstring libcefPath = cefDir + L"\\libcef.dll";
    DWORD fileAttrib = GetFileAttributesW(libcefPath.c_str());
    if (fileAttrib == INVALID_FILE_ATTRIBUTES) {
        logMessage("Windows", stdext::format("ERROR: libcef.dll not found at %s", 
            std::string(libcefPath.begin(), libcefPath.end())).c_str());
        logMessage("Windows", "Make sure to copy the CEF runtime files to the ./cef/ directory");
        return;
    }

    CefString(&settings.cache_path) = cacheDir;
    CefString(&settings.root_cache_path) = cacheDir;
    CefString(&settings.browser_subprocess_path) = subprocessPath;
}

void CefConfigWindows::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void CefConfigWindows::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    applyGenericCommandLineFlags(command_line);
    
    // Use fixed cache directory to avoid path length issues
    CreateDirectoryA("C:\\cef_temp", nullptr);
    command_line->AppendSwitchWithValue("disk-cache-dir", "C:\\cef_temp");
    
    logMessage("Windows", "Using fixed cache directory: C:\\cef_temp");
    logMessage("Windows", stdext::format("Command line flags: %s",
        command_line->GetCommandLineString().ToString()).c_str());
}

CefMainArgs CefConfigWindows::createMainArgs(int argc, const char* argv[]) {
    return CefMainArgs(GetModuleHandle(nullptr));
}

bool CefConfigWindows::handleSubprocessExecution(const CefMainArgs& args, CefRefPtr<CefApp> app) {
    setupDllDirectories();
    // Early-subprocess exit (the main executable should never be used as subprocess
    // when browser_subprocess_path is defined, but call CefExecuteProcess for completeness)
    const int code = CefExecuteProcess(args, nullptr, nullptr);
    logMessage("Windows", stdext::format("CefExecuteProcess returned code: %d", code).c_str());
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
    logMessage("Windows", "Scheme handlers registered");
#else
    logMessage("Windows", "Skipping scheme handler registration in subprocess");
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