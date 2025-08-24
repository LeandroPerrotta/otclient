#include "cef_app.h"

#ifdef USE_CEF

#include <framework/stdext/format.h>
#include "include/wrapper/cef_helpers.h"

// Forward declaration of rawLogger from cef_init.cpp
// TODO: This will be moved to cef_logger.h in a future refactoring step
void rawLogger(const char* message);

// Browser process handler implementation
void OTClientBrowserProcessHandler::OnScheduleMessagePumpWork(int64_t delay_ms) {
    // This method is called when work is scheduled for the browser process main thread
    // With multi_threaded_message_loop = true, CEF manages its own threading
}

// OTClientBrowserApp implementation
OTClientBrowserApp::OTClientBrowserApp() {
    m_browserProcessHandler = new OTClientBrowserProcessHandler();
}

CefRefPtr<CefBrowserProcessHandler> OTClientBrowserApp::GetBrowserProcessHandler() {
    return m_browserProcessHandler;
}

void OTClientBrowserApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    registrar->AddCustomScheme("otclient",
                               CEF_SCHEME_OPTION_STANDARD |
                               CEF_SCHEME_OPTION_LOCAL |
                               CEF_SCHEME_OPTION_DISPLAY_ISOLATED);
}

void OTClientBrowserApp::OnBeforeCommandLineProcessing(const CefString& process_type,
                                                       CefRefPtr<CefCommandLine> command_line) {
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    command_line->AppendSwitch("angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    command_line->AppendSwitch("shared-texture-enabled");

    // Essential flags for GPU acceleration
    command_line->AppendSwitch("disable-gpu-watchdog"); // Prevent GPU process timeout     
#else
    command_line->AppendSwitchWithValue("ozone-platform", "x11");

    // Enable GPU acceleration across processes
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-compositing");
    command_line->AppendSwitch("enable-gpu-rasterization");
    command_line->AppendSwitch("disable-software-rasterizer");
    command_line->AppendSwitch("disable-gpu-sandbox"); // Sometimes needed for shared textures

    command_line->AppendSwitch("enable-begin-frame-scheduling");
    command_line->AppendSwitch("disable-background-timer-throttling");
    command_line->AppendSwitch("disable-renderer-backgrounding");

    // Log the command line AFTER all switches have been added
    rawLogger(stdext::format("cmline: %s", process_type.ToString()).c_str());
    rawLogger(stdext::format("Command line flags set to: %s", command_line->GetCommandLineString()).c_str());
#endif
}

#endif // USE_CEF