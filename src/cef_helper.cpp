#include <windows.h>
#include <string>
#include <shellapi.h>       // Para CommandLineToArgvW
#include <libloaderapi.h>   // Para SetDllDirectoryW
#include "include/cef_app.h"

// Definição mínima da mesma classe CEF App usada no processo principal
#ifdef USE_CEF
#include "include/cef_render_process_handler.h"

class OTClientCEFApp : public CefApp, public CefRenderProcessHandler {
public:
    OTClientCEFApp() {}

    // CefApp methods
    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    // CefRenderProcessHandler methods  
    virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) override {
        // Implementação mínima para compatibilidade
    }

private:
    IMPLEMENT_REFCOUNTING(OTClientCEFApp);
};
#endif

static std::wstring GetModuleDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

static void SetupDllSearch(const std::wstring& dir) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto pSetDefaultDllDirectories = (BOOL (WINAPI*)(DWORD))GetProcAddress(k32, "SetDefaultDllDirectories");
    auto pAddDllDirectory         = (PVOID (WINAPI*)(PCWSTR))GetProcAddress(k32, "AddDllDirectory");
    auto pSetDllDirectoryW        = (BOOL (WINAPI*)(LPCWSTR))GetProcAddress(k32, "SetDllDirectoryW");
    
    if (pSetDefaultDllDirectories && pAddDllDirectory) {
        pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
        if (pSetDllDirectoryW) pSetDllDirectoryW(L""); // opcional, remove diretório atual
        pAddDllDirectory(dir.c_str());                 // adiciona .\cef (onde o helper está)
    } else if (pSetDllDirectoryW) {
        // fallback usando GetProcAddress também
        pSetDllDirectoryW(dir.c_str());
    }
    // Se nenhuma função disponível, continue sem configurar (CEF deve funcionar mesmo assim)
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Se o helper for aberto manualmente (sem --type=...), finalize silencioso.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool hasType = false;
    for (int i = 0; i < argc; ++i) {
        if (wcsstr(argv[i], L"--type=") != nullptr) { hasType = true; break; }
    }
    if (!hasType) return 0;

    // Isola DLLs no diretório do helper (que está dentro de .\cef)
    const std::wstring myDir = GetModuleDir();
    SetupDllSearch(myDir);

    CefMainArgs main_args(hInst);
    CefRefPtr<CefApp> app = new OTClientCEFApp(); // mesma classe do host
    return CefExecuteProcess(main_args, app, nullptr);
}