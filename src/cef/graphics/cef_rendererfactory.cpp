#include "cef_rendererfactory.h"
#include "cef_renderercpu.h"
#include "gpu/cef_renderergpulinuxmesa.h"
#include "gpu/cef_renderergpulinuxnonmesa.h"
#include "gpu/cef_renderergpuwin.h"

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
