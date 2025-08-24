#pragma once

#ifdef USE_CEF
bool InitializeCEF(int argc, const char* argv[]);
void ShutdownCEF();
void rawLogger(const char* message);
extern bool g_cefInitialized;
#else
inline bool InitializeCEF(int, const char**) { return true; }
inline void ShutdownCEF() {}
inline void rawLogger(const char*) {}
inline bool g_cefInitialized = false;
#endif

