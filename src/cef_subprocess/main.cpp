#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_message_router.h"
#include "../framework/stdext/format.h"

#ifdef _WIN32
#  include <windows.h>
#endif

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
    printf("[CEF-subprocess] %s\n", message);
    fflush(stdout);
}

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
        rawLogger(stdext::format("SUBPROCESS OnBeforeCommandLineProcessing called for process: %s", process_type.ToString()).c_str());
        
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
        // Minimal ANGLE configuration that works (same as main process)
        command_line->AppendSwitch("angle");
        command_line->AppendSwitchWithValue("use-angle", "d3d11");
        // TODO: Implement GetGpuLuid() if needed for multi-GPU systems
        // command_line->AppendSwitchWithValue("use-adapter-luid", this->GetGpuLuid());
#else
        // Linux-specific OpenGL flags and GPU setup (same as main process)
        rawLogger("SUBPROCESS Configuring GPU settings for Linux...");
        
        // Force GPU acceleration - try multiple strategies
        // Strategy 1: Force specific GL implementation
        command_line->AppendSwitchWithValue("use-gl", "egl");
        
        // Strategy 2: Force platform
        command_line->AppendSwitchWithValue("ozone-platform", "x11");
        
        // Strategy 3: Aggressive GPU forcing
        command_line->AppendSwitch("enable-gpu");
        command_line->AppendSwitch("enable-gpu-compositing");  
        command_line->AppendSwitch("enable-gpu-rasterization");
        command_line->AppendSwitch("disable-software-rasterizer");
        command_line->AppendSwitch("disable-gpu-sandbox");
        
        // Strategy 4: Ignore all blocklists and workarounds
        command_line->AppendSwitch("ignore-gpu-blocklist");
        command_line->AppendSwitch("ignore-gpu-blacklist");
        command_line->AppendSwitch("disable-gpu-driver-bug-workarounds");
        command_line->AppendSwitch("disable-software-compositing-fallback");
        
        // Strategy 5: Force hardware acceleration
        command_line->AppendSwitch("enable-accelerated-2d-canvas");
        command_line->AppendSwitch("enable-accelerated-video-decode");
        command_line->AppendSwitch("force-gpu-rasterization");
        
        // Strategy 6: Disable problematic features in CEF 139+
        command_line->AppendSwitch("disable-features=VizDisplayCompositor,UseSkiaRenderer");
        
        // Strategy 7: Force specific GPU preferences
        if (process_type.ToString() == "gpu-process") {
            rawLogger("SUBPROCESS Applying GPU-process specific settings...");
            command_line->AppendSwitch("enable-unsafe-webgpu");
            command_line->AppendSwitch("use-cmd-decoder=passthrough");
        }
#endif
        
        // Performance flags for all processes (not GPU-specific)
        command_line->AppendSwitch("enable-begin-frame-scheduling");
        command_line->AppendSwitch("disable-background-timer-throttling");
        command_line->AppendSwitch("disable-renderer-backgrounding");

        // Log the command line AFTER all switches have been added
        rawLogger(stdext::format("SUBPROCESS cmline: %s", process_type.ToString()).c_str());
        rawLogger(stdext::format("SUBPROCESS Command line flags set to: %s", command_line->GetCommandLineString()).c_str());
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

