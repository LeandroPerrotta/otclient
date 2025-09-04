#include "cef_confwin.h"

#ifdef USE_CEF
#ifdef _WIN32

#include "cef_helper.h"
#include <framework/stdext/format.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>
#include <dxgi1_2.h>

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
    const std::wstring cefDir = getExecutableDirectory() + L"\\cef";
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    AddDllDirectory(cefDir.c_str());
}

void CefConfigWindows::configurePaths(CefSettings& settings) {
    setupDllDirectories();
    const std::wstring exeDir = getExecutableDirectory();
    const std::wstring cefDir = exeDir + L"\\cef";
    const std::wstring localesDir = cefDir + L"\\locales";
    const std::wstring cacheDir = cefDir + L"\\cache";
    const std::wstring subprocessPath = cefDir + L"\\otclient_cef_subproc.exe";

    CefString(&settings.resources_dir_path) = cefDir;
    CefString(&settings.locales_dir_path) = localesDir;
    CefString(&settings.cache_path) = cacheDir;
    CefString(&settings.root_cache_path) = cacheDir;
    CefString(&settings.browser_subprocess_path) = subprocessPath;

    logMessage("Windows", stdext::format("CEF directory: %s", std::string(cefDir.begin(), cefDir.end())).c_str());
}

void CefConfigWindows::applySettings(CefSettings& settings) {
    applyGenericSettings(settings);
    configurePaths(settings);
}

void CefConfigWindows::applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) {
    applyGenericCommandLineFlags(command_line);
    
    // Apply Intel-specific workarounds only if Intel graphics detected
    if (isIntelGraphicsSystem()) {
        logMessage("Windows", "Intel graphics detected - applying compatibility workarounds");
        
        // Critical Intel graphics workarounds to prevent GPU subprocess crashes
        // These flags specifically target the initialization crash in Intel graphics
        
        // Disable D3D11 which is problematic with Intel drivers during CEF subprocess init
        command_line->AppendSwitch("disable-d3d11");
        
        // Force software compositing for Intel to prevent GPU process crash
        command_line->AppendSwitch("disable-gpu-compositing");
        
        // Disable ANGLE backend that causes Intel crashes during initialization
        command_line->AppendSwitch("use-gl=desktop");
        
        // Disable specific Intel problematic features that crash during subprocess init
        command_line->AppendSwitch("disable-features=VizDisplayCompositor,D3D11VideoDecoder");
        
        // Enable safer rendering path
        command_line->AppendSwitch("enable-features=UseSkiaRenderer");
        
        // Prevent GPU process restart loops
        command_line->AppendSwitch("disable-gpu-process-crash-limit");
        
        // Additional Intel-specific stability flags
        command_line->AppendSwitch("disable-gpu-driver-bug-workarounds");
        command_line->AppendSwitch("disable-accelerated-2d-canvas");
        command_line->AppendSwitch("disable-accelerated-video-decode");
        
        logMessage("Windows", "Intel compatibility flags applied");
    }

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

bool CefConfigWindows::shouldDisableGPUForIntelGraphics() const {
    // By default, disable GPU acceleration for Intel graphics to prevent CEF crashes
    // This can be overridden by setting allow_intel_graphics_override to true
    std::string override_setting = getUserPreference("allow_intel_graphics_override");
    if (override_setting == "true" || override_setting == "1") {
        return false; // User explicitly wants to enable Intel GPU acceleration
    }
    return true; // Default: disable for Intel graphics
}

bool CefConfigWindows::shouldAllowIntelGraphicsOverride() const {
    // Check if user has explicitly enabled Intel graphics override
    std::string override_setting = getUserPreference("allow_intel_graphics_override");
    return (override_setting == "true" || override_setting == "1");
}

bool CefConfigWindows::isIntelGraphicsSystem() const {
    // Check Windows registry and system info for Intel graphics
    // This needs to be done before OpenGL context creation
    
    // Method 1: Check DXGI adapters
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (SUCCEEDED(hr)) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                // Check if vendor is Intel (VendorId = 0x8086)
                if (desc.VendorId == 0x8086) {
                    std::wstring description(desc.Description);
                    std::string descStr(description.begin(), description.end());
                    logMessage("Windows", stdext::format("Intel graphics adapter detected: %s", descStr.c_str()).c_str());
                    adapter->Release();
                    factory->Release();
                    return true;
                }
            }
            adapter->Release();
        }
        factory->Release();
    }
    
    // Method 2: Check via WMI (fallback)
    // This is more complex but could be added if needed
    
    return false;
}

} // namespace cef

#endif // _WIN32
#endif // USE_CEF