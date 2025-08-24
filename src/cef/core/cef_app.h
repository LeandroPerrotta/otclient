#pragma once

#ifdef USE_CEF

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_command_line.h"
#include "include/cef_scheme.h"

// Browser process handler for proper message pump work scheduling
class OTClientBrowserProcessHandler : public CefBrowserProcessHandler {
public:
    void OnScheduleMessagePumpWork(int64_t delay_ms) override;

    IMPLEMENT_REFCOUNTING(OTClientBrowserProcessHandler);
};

// CEF app used by the browser process to register custom schemes and handle command line processing
class OTClientBrowserApp : public CefApp {
private:
    CefRefPtr<OTClientBrowserProcessHandler> m_browserProcessHandler;

public:
    OTClientBrowserApp();

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

    IMPLEMENT_REFCOUNTING(OTClientBrowserApp);
};

#endif // USE_CEF