#include "windows_gpu_context.h"

void WindowsGPUContext::initialize()
{
#if defined(USE_CEF) && defined(_WIN32)
    // No-op for now; resources are created per-frame in renderer
#endif
}

void WindowsGPUContext::cleanup()
{
#if defined(USE_CEF) && defined(_WIN32)
    // No shared resources to release
#endif
}

