#include "cef_rendererfactory.h"
#include "cef_renderercpu.h"
#if defined(__linux__)
#include "gpu/cef_renderergpulinuxmesa.h"
#include "gpu/cef_renderergpulinuxnonmesa.h"
#endif
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
#include "gpu/cef_renderergpuwin.h"
#endif

std::unique_ptr<CefRenderer> CefRendererFactory::createRenderer(UICEFWebView& view)
{

#if defined(USE_CEF)
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    g_logger.info("CefRendererFactory: Checking for GPU Windows renderer");
    {
        auto renderer = std::make_unique<CefRendererGPUWin>(view);
        if(renderer->isSupported())
            return renderer;
    }
#endif
#if defined(__linux__)
    g_logger.info("CefRendererFactory: Checking for GPU Linux renderer");
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
    g_logger.info("CefRendererFactory: Creating CPU renderer");
    return std::make_unique<CefRendererCPU>(view);
#else
    (void)view;
    return nullptr;
#endif
}
