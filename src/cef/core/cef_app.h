#pragma once

#ifdef USE_CEF

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_command_line.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_message_router.h"

// Base class for OTClient CEF applications with shared functionality
class OTClientAppBase : public CefApp {
public:
    // Shared functionality between browser and render processes
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

protected:
    // Helper method for logging with process-specific prefix
    virtual std::string getLogPrefix() const = 0;

    IMPLEMENT_REFCOUNTING(OTClientAppBase);
};

// Browser process handler for proper message pump work scheduling
class OTClientBrowserProcessHandler : public CefBrowserProcessHandler {
public:
    void OnScheduleMessagePumpWork(int64_t delay_ms) override;

    IMPLEMENT_REFCOUNTING(OTClientBrowserProcessHandler);
};

// CEF app used by the browser process
class OTClientBrowserApp : public OTClientAppBase {
private:
    CefRefPtr<OTClientBrowserProcessHandler> m_browserProcessHandler;

public:
    OTClientBrowserApp();

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;

protected:
    std::string getLogPrefix() const override;

    IMPLEMENT_REFCOUNTING(OTClientBrowserApp);
};

// CEF app used by the render process (subprocess)
class OTClientRenderApp : public OTClientAppBase, public CefRenderProcessHandler {
public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

    // Render process specific methods
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override;
    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) override;
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override;

protected:
    std::string getLogPrefix() const override;

private:
    CefRefPtr<CefMessageRouterRendererSide> m_messageRouter;

    IMPLEMENT_REFCOUNTING(OTClientRenderApp);
};

#endif // USE_CEF