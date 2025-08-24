#pragma once

#ifdef USE_CEF
#ifndef _WIN32

#include "cef_config.h"
#include <string>

namespace cef {

// Linux-specific CEF configuration
class LinuxCefConfig : public CefConfig {
public:
    void applySettings(CefSettings& settings) override;
    void applyCommandLineFlags(CefRefPtr<CefCommandLine> command_line) override;
    void configurePaths(CefSettings& settings) override;
    std::string getPlatformName() const override { return "Linux"; }

private:
    std::string findCefDirectory() const;
    void configureX11(CefRefPtr<CefCommandLine> command_line);
};

} // namespace cef

#endif // !_WIN32
#endif // USE_CEF