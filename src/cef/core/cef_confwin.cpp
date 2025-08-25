#include "cef_confwin.h"

#ifdef USE_CEF
#ifdef _WIN32

#include "cef_helper.h"
#include <framework/stdext/format.h>
#include <cef/resources/cefphysfsresourcehandler.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>
#include "include/cef_scheme.h"

namespace cef {

std::wstring CefConfigWindows::getExecutableDirectory() const {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (!n || n >= MAX_PATH) return L".";
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

void CefConfigWindows::configurePaths(CefSettings& settings) {
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

void CefConfigWindows::configureAngle(CefRefPtr<CefCommandLine> command_line) {
#if defined(OPENGL_ES) && OPENGL_ES == 2
    command_line->AppendSwitch("angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    command_line->AppendSwitch("shared-texture-enabled");
    command_line->AppendSwitch("disable-gpu-watchdog"); // Prevent GPU process timeout
    
    logMessage("Windows", "ANGLE D3D11 configuration applied");
#endif
}

void CefConfigWindows::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void CefConfigWindows::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    configureAngle(command_line);
    applyGenericCommandLineFlags(command_line);
    
    logMessage("Windows", stdext::format("Command line flags: %s", 
        command_line->GetCommandLineString().ToString()).c_str());
}

CefMainArgs CefConfigWindows::createMainArgs(int argc, const char* argv[]) {
    return CefMainArgs(GetModuleHandle(nullptr));
}

bool CefConfigWindows::handleSubprocessExecution(const CefMainArgs& args, CefRefPtr<CefApp> app) {
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
    CefRegisterSchemeHandlerFactory("otclient", "", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("http", "otclient", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("https", "otclient", new CefPhysFsSchemeHandlerFactory);
    logMessage("Windows", "Scheme handlers registered");
}

} // namespace cef

#endif // _WIN32
#endif // USE_CEF