#include <cef/core/cef_init.h>
#include <cef/core/cef_app.h>
#include <cef/core/cef_helper.h>
#include <cef/core/cef_config.h>
#include <cef/resources/cefphysfsresourcehandler.h>
#include <cef/ui/uicefwebview.h>
#include <framework/core/logger.h>
#include <framework/core/resourcemanager.h>
#include <framework/stdext/format.h>
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"
#ifdef _WIN32
#include <windows.h>
#endif

// Global CEF state
bool g_cefInitialized = false;





// Global CEF initialization function (simplified)
bool InitializeCEF(int argc, const char* argv[]) {

    // Create platform-specific configuration
    auto config = cef::CefConfigFactory::createConfig();
    if (!config) {
        cef::logMessage("FAILED to create CEF configuration!");
        return false;
    }

    cef::logMessage(config->getPlatformName().c_str(), "Starting CEF initialization");

    // Create CEF app first (needed for subprocess execution)
    CefRefPtr<CefApp> app = new OTClientBrowserApp();

#ifdef _WIN32
    // Early-subprocess exit (the main executable should never be used as subprocess
    // when browser_subprocess_path is defined, but call CefExecuteProcess for
    // completeness)
    CefMainArgs main_args(GetModuleHandle(nullptr));
    {
        const int code = CefExecuteProcess(main_args, nullptr, nullptr);
        cef::logMessage(("CefExecuteProcess returned code: " + std::to_string(code)).c_str());
        if (code >= 0)
            std::exit(code);
    }

    // Configure command line flags using configuration system
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    command_line->InitFromArgv(argc, argv);
    config->applyCommandLineFlags(command_line);
#else
    CefMainArgs main_args(argc, const_cast<char**>(argv));
    
    // For Linux, check for subprocess execution BEFORE configuring settings
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        std::exit(exit_code);
    }
#endif

    // Configure CEF settings using configuration system
    CefSettings settings;
    config->applySettings(settings);

    bool result = CefInitialize(main_args, settings, app, nullptr);

    if (!result) {
        cef::logMessage("FAILED to initialize CEF!");
        return false;
    }

    // Register scheme handlers
    CefRegisterSchemeHandlerFactory("otclient", "", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("http", "otclient", new CefPhysFsSchemeHandlerFactory);
    CefRegisterSchemeHandlerFactory("https", "otclient", new CefPhysFsSchemeHandlerFactory);
    g_cefInitialized = true;
    cef::logMessage("CEF initialized and scheme handlers registered");

    return true;
}

void ShutdownCEF() {
    if (g_cefInitialized) {
        cef::logMessage("Starting CEF shutdown...");

        // Close all active WebViews first
        UICEFWebView::closeAllWebViews();

        // With multi_threaded_message_loop = true, CEF manages its own shutdown

        cef::logMessage("All webviews closed... Shutting down CEF");

        // Shutdown CEF
        CefShutdown();
        g_cefInitialized = false;
        cef::logMessage("CEF shutdown completed");
    }
}
