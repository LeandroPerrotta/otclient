#include <cef/core/cef_init.h>
#include <cef/core/cef_app.h>
#include <cef/core/cef_helper.h>
#include <cef/core/cef_config.h>
#include <cef/ui/uicefwebview.h>
#include <framework/stdext/format.h>
#include "include/cef_app.h"
#include <exception>

// Global CEF state
bool g_cefInitialized = false;

/**
 * Initialize CEF with platform-specific configuration
 * 
 * This function handles the complete CEF initialization process:
 * 1. Creates platform-specific configuration
 * 2. Handles subprocess execution (if needed)
 * 3. Configures CEF settings and initializes CEF
 * 4. Registers custom scheme handlers
 * 
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return true if initialization succeeded, false otherwise
 */
bool InitializeCEF(int argc, const char* argv[]) {
    // Create platform-specific configuration
    auto config = cef::CefConfigFactory::createConfig();
    if (!config) {
        cef::logMessage("ERROR", "Failed to create CEF configuration!");
        return false;
    }

    cef::logMessage(config->getPlatformName().c_str(), "Starting CEF initialization");

    try {
        // Create CEF app and main args using platform-specific config
        CefRefPtr<CefApp> app = new OTClientBrowserApp();
        CefMainArgs main_args = config->createMainArgs(argc, argv);
        
        // Handle subprocess execution (platform-specific, may exit)
        config->handleSubprocessExecution(main_args, app);
        
        // Configure CEF settings and initialize
        CefSettings settings;
        config->applySettings(settings);
        
        cef::logMessage(config->getPlatformName().c_str(), "Initializing CEF with configured settings");
        bool result = CefInitialize(main_args, settings, app, nullptr);
        
        if (!result) {
            cef::logMessage("ERROR", "CefInitialize failed!");
            return false;
        }

        // Register custom scheme handlers
        config->registerSchemeHandlers();
        
        g_cefInitialized = true;
        cef::logMessage(config->getPlatformName().c_str(), "CEF initialization completed successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        cef::logMessage("ERROR", stdext::format("CEF initialization failed with exception: %s", e.what()).c_str());
        return false;
    } catch (...) {
        cef::logMessage("ERROR", "CEF initialization failed with unknown exception");
        return false;
    }
}

/**
 * Shutdown CEF and cleanup resources
 * 
 * This function handles the complete CEF shutdown process:
 * 1. Closes all active WebViews
 * 2. Shuts down CEF
 * 3. Resets global state
 */
void ShutdownCEF() {
    if (!g_cefInitialized) {
        return;
    }
    
    cef::logMessage("CEF", "Starting CEF shutdown...");

    try {
        // Close all active WebViews first
        UICEFWebView::closeAllWebViews();
        cef::logMessage("CEF", "All WebViews closed");

        // With multi_threaded_message_loop = true, CEF manages its own shutdown
        CefShutdown();
        
        g_cefInitialized = false;
        cef::logMessage("CEF", "CEF shutdown completed successfully");
        
    } catch (const std::exception& e) {
        cef::logMessage("ERROR", stdext::format("CEF shutdown failed with exception: %s", e.what()).c_str());
        g_cefInitialized = false; // Reset state even on error
    } catch (...) {
        cef::logMessage("ERROR", "CEF shutdown failed with unknown exception");
        g_cefInitialized = false; // Reset state even on error
    }
}