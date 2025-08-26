#pragma once

#ifdef USE_CEF
#include <memory>
bool InitializeCEF(int argc, const char* argv[]);
void ShutdownCEF();
extern bool g_cefInitialized;
namespace cef { class CefConfig; }
extern std::unique_ptr<cef::CefConfig> g_cefConfig;
#else
inline bool InitializeCEF(int, const char**) { return true; }
inline void ShutdownCEF() {}
inline bool g_cefInitialized = false;
#endif

