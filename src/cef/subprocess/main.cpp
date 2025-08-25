#include "../core/cef_app.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#if defined(_WIN32)
// Windows entry point. Using wWinMain ensures wide-character arguments and
// provides the HINSTANCE handle required by CefMainArgs on Windows builds.
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    CefMainArgs main_args(hInstance);
    CefRefPtr<OTClientRenderApp> app(new OTClientRenderApp);
    return CefExecuteProcess(main_args, app, nullptr);
}
#else
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    CefRefPtr<OTClientRenderApp> app(new OTClientRenderApp);
    return CefExecuteProcess(main_args, app, nullptr);
}
#endif

