#include "cef_app.h"

#ifdef USE_CEF

#include "cef_helper.h"
#include "cef_config.h"
#include <framework/stdext/format.h>
#include "include/wrapper/cef_helpers.h"

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
    // Use the configuration system to apply platform-specific command line flags
    auto config = cef::CefConfigFactory::createConfig();
    if (config) {
        config->applyCommandLineFlags(command_line);
        cef::logMessage(config->getPlatformName().c_str(), 
                       stdext::format("Process type: %s", process_type.ToString()).c_str());
    } else {
        cef::logMessage("ERROR", "Failed to create CEF configuration for command line processing");
    }
}

#endif // USE_CEF