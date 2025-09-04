#pragma once

#ifdef USE_CEF
#ifdef _WIN32

#include "cef_config.h"
#include <string>

namespace cef {

// Windows-specific CEF configuration
class CefConfigWindows : public CefConfig {
public:
    CefConfigWindows();
    void applySettings(CefSettings& settings) override;
    void applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) override;
    void configurePaths(CefSettings& settings) override;
    CefMainArgs createMainArgs(int argc, const char* argv[]) override;
    bool handleSubprocessExecution(const CefMainArgs& args, CefRefPtr<CefApp> app) override;
    void registerSchemeHandlers() override;
    bool shouldUseSharedTexture() const override;
    bool shouldDisableGPUForIntelGraphics() const override;
    bool shouldAllowIntelGraphicsOverride() const override;
    std::string getPlatformName() const override { return "Windows"; }

private:
    std::wstring getExecutableDirectory() const;
    void setupDllDirectories() const;
};

} // namespace cef

#endif // _WIN32
#endif // USE_CEF