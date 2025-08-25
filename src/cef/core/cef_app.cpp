#include "cef_app.h"

#ifdef USE_CEF

#include "cef_helper.h"
#include "cef_config.h"
#include <framework/stdext/format.h>
#include "include/wrapper/cef_helpers.h"

// ============================================================================
// OTClientAppBase Implementation (Shared functionality)
// ============================================================================

void OTClientAppBase::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
    registrar->AddCustomScheme("otclient",
                               CEF_SCHEME_OPTION_STANDARD |
                               CEF_SCHEME_OPTION_LOCAL |
                               CEF_SCHEME_OPTION_DISPLAY_ISOLATED);
}

void OTClientAppBase::OnBeforeCommandLineProcessing(const CefString& process_type,
                                                    CefRefPtr<CefCommandLine> command_line) {
    // Use the configuration system to apply platform-specific command line flags
    auto config = cef::CefConfigFactory::createConfig();
    if (config) {
        config->applyCommandLineFlags(command_line);
        cef::logMessage(getLogPrefix().c_str(), 
                       stdext::format("Process type: %s on %s", 
                                    process_type.ToString(), 
                                    config->getPlatformName()).c_str());
    } else {
        cef::logMessage(getLogPrefix().c_str(), "ERROR: Failed to create CEF configuration");
    }
}

// ============================================================================
// Browser Process Handler Implementation
// ============================================================================

void OTClientBrowserProcessHandler::OnScheduleMessagePumpWork(int64_t delay_ms) {
    // This method is called when work is scheduled for the browser process main thread
    // With multi_threaded_message_loop = true, CEF manages its own threading
}

// ============================================================================
// OTClientBrowserApp Implementation (Browser-specific functionality)
// ============================================================================

OTClientBrowserApp::OTClientBrowserApp() {
    m_browserProcessHandler = new OTClientBrowserProcessHandler();
}

CefRefPtr<CefBrowserProcessHandler> OTClientBrowserApp::GetBrowserProcessHandler() {
    return m_browserProcessHandler;
}

std::string OTClientBrowserApp::getLogPrefix() const {
    return "CEF-browser";
}

// ============================================================================
// OTClientRenderApp Implementation (Render-specific functionality)
// ============================================================================

CefRefPtr<CefRenderProcessHandler> OTClientRenderApp::GetRenderProcessHandler() {
    return this;
}

std::string OTClientRenderApp::getLogPrefix() const {
    return "CEF-subprocess";
}

void OTClientRenderApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefV8Context> context) {
    if (!m_messageRouter) {
        CefMessageRouterConfig config;
        config.js_query_function = "luaCallback";
        config.js_cancel_function = "luaCallbackCancel";
        m_messageRouter = CefMessageRouterRendererSide::Create(config);
    }
    m_messageRouter->OnContextCreated(browser, frame, context);
}

void OTClientRenderApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefV8Context> context) {
    if (m_messageRouter)
        m_messageRouter->OnContextReleased(browser, frame, context);
}

bool OTClientRenderApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                 CefRefPtr<CefFrame> frame,
                                                 CefProcessId source_process,
                                                 CefRefPtr<CefProcessMessage> message) {
    if (m_messageRouter &&
        m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message))
        return true;
    return false;
}

#endif // USE_CEF