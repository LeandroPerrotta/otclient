#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_message_router.h"

#ifdef _WIN32
#  include <windows.h>
#endif

// Render process application used in the dedicated subprocess. It mirrors the
// behaviour that previously lived in the main executable when running in
// single-process mode.
class OTClientRenderApp : public CefApp, public CefRenderProcessHandler {
public:
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
        registrar->AddCustomScheme("otclient",
                                   CEF_SCHEME_OPTION_STANDARD |
                                   CEF_SCHEME_OPTION_LOCAL |
                                   CEF_SCHEME_OPTION_DISPLAY_ISOLATED);
    }

    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override {
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
        // Minimal ANGLE configuration that works (same as main process)
        command_line->AppendSwitch("angle");
        command_line->AppendSwitchWithValue("use-angle", "d3d11");
        // TODO: Implement GetGpuLuid() if needed for multi-GPU systems
        // command_line->AppendSwitchWithValue("use-adapter-luid", this->GetGpuLuid());
#endif
        
        // Performance flags for all processes (not GPU-specific)
        // command_line->AppendSwitch("enable-begin-frame-scheduling");
        // command_line->AppendSwitch("disable-background-timer-throttling");
        // command_line->AppendSwitch("disable-renderer-backgrounding");
    }

    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        if (!m_messageRouter) {
            CefMessageRouterConfig config;
            config.js_query_function = "luaCallback";
            config.js_cancel_function = "luaCallbackCancel";
            m_messageRouter = CefMessageRouterRendererSide::Create(config);
        }
        m_messageRouter->OnContextCreated(browser, frame, context);
    }

    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) override {
        if (m_messageRouter)
            m_messageRouter->OnContextReleased(browser, frame, context);
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override {
        if (m_messageRouter &&
            m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message))
            return true;
        return false;
    }

private:
    CefRefPtr<CefMessageRouterRendererSide> m_messageRouter;

    IMPLEMENT_REFCOUNTING(OTClientRenderApp);
};

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

