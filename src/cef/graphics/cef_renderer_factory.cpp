#include "cef_renderer_factory.h"
#include "cef_renderer_cpu.h"
#include "gpu/cef_renderer_gpu_linux_mesa.h"
#include "gpu/cef_renderer_gpu_linux_nonmesa.h"
#include "gpu/cef_renderer_gpu_win.h"

std::unique_ptr<CefRenderer> CefRendererFactory::createRenderer(UICEFWebView& view)
{
#if defined(USE_CEF)
#if defined(_WIN32)
    {
        auto renderer = std::make_unique<CefRendererGPUWin>(view);
        if(renderer->isSupported())
            return renderer;
    }
#endif
#if defined(__linux__)
    {
        auto renderer = std::make_unique<CefRendererGPULinuxMesa>(view);
        if(renderer->isSupported())
            return renderer;
    }
    {
        auto renderer = std::make_unique<CefRendererGPULinuxNonMesa>(view);
        if(renderer->isSupported())
            return renderer;
    }
#endif
    return std::make_unique<CefRendererCPU>(view);
#else
    (void)view;
    return nullptr;
#endif
}
