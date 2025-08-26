#include "cef_renderergpuwin.h"
#include "../../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include "../../core/cef_init.h"
#include "../../core/cef_config.h"
#if defined(USE_CEF)
#include <include/cef_browser.h>
#endif
#if defined(USE_CEF) && defined(_WIN32)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#endif

CefRendererGPUWin::CefRendererGPUWin(UICEFWebView& view)
    : CefRenderer(view)
    , m_lastWidth(0)
    , m_lastHeight(0)
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    , m_angleD3D11Device(nullptr)
    , m_angleD3D11Device1(nullptr)
    , m_angleD3D11Context(nullptr)
    , m_angleDestTexture(nullptr)
    , m_classicSharedHandle(nullptr)
    , m_pbuffer(EGL_NO_SURFACE)
    , m_pbufferBound(false)
    , eglQueryDisplayAttribEXT(nullptr)
    , eglQueryDeviceAttribEXT(nullptr)
#endif
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    if (!initializeAngleInterop()) {
        g_logger.error("CefRendererGPUWin: Failed to initialize ANGLE interop");
    }
#endif
}

CefRendererGPUWin::~CefRendererGPUWin()
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    cleanupResources();
#endif
}

void CefRendererGPUWin::onPaint(const void* buffer, int width, int height,
                                const CefRenderHandler::RectList& dirtyRects)
{
    (void)buffer; (void)width; (void)height; (void)dirtyRects;
}

void CefRendererGPUWin::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    HANDLE ntHandle = static_cast<HANDLE>(info.shared_texture_handle);
    if (!ntHandle || ntHandle == INVALID_HANDLE_VALUE) {
        g_logger.error("CefRendererGPUWin: Invalid shared texture handle");
        return;
    }

    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;

    g_logger.debug(stdext::format("CefRendererGPUWin: Processing NT handle %p, size: %dx%d", 
                                  ntHandle, width, height));

    // Move operations to main thread where OpenGL context lives
    g_dispatcher.addEventFromOtherThread([this, ntHandle, width, height]() {
        if (!m_angleD3D11Device) {
            g_logger.error("CefRendererGPUWin: ANGLE D3D11 device not available");
            return;
        }

        // Check if we need to recreate destination texture for new size
        if (!m_angleDestTexture || m_lastWidth != width || m_lastHeight != height) {
            if (!createAngleDestinationTexture(width, height)) {
                g_logger.error("CefRendererGPUWin: Failed to create ANGLE destination texture");
                return;
            }
            
            if (!setupEGLPbuffer()) {
                g_logger.error("CefRendererGPUWin: Failed to setup EGL pbuffer");
                return;
            }
            
            // Create OpenGL texture
            m_cefTexture = TexturePtr(new Texture(Size(width, height)));
            m_lastWidth = width;
            m_lastHeight = height;
            m_textureCreated = true;
            
            g_logger.info(stdext::format("CefRendererGPUWin: Created new texture %dx%d", width, height));
        }

        // Copy from CEF's NT handle to our classic handle texture
        if (!copyFromCEFTexture(ntHandle)) {
            g_logger.error("CefRendererGPUWin: Failed to copy from CEF texture");
            return;
        }

        g_logger.debug("CefRendererGPUWin: Successfully processed frame");
    });
#else
    (void)info;
#endif
}

bool CefRendererGPUWin::isSupported() const
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    g_logger.info("CefRendererGPUWin: Checking GPU acceleration support...");
    
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        g_logger.error("CefRendererGPUWin: No current EGL display - GPU acceleration not supported");
        return false;
    }
    
    g_logger.info("CefRendererGPUWin: EGL display available");
    
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!extensions) {
        g_logger.error("CefRendererGPUWin: Failed to query EGL extensions - GPU acceleration not supported");
        return false;
    }
    
    // Check for required extensions
    bool hasD3DExtension = strstr(extensions, "EGL_ANGLE_d3d_share_handle_client_buffer") != nullptr;
    bool hasDeviceQuery = strstr(extensions, "EGL_EXT_device_query") != nullptr;
    bool hasAngleDevice = strstr(extensions, "EGL_ANGLE_device_d3d") != nullptr;
    
    if (!hasD3DExtension) {
        g_logger.error("CefRendererGPUWin: EGL_ANGLE_d3d_share_handle_client_buffer extension not found");
        return false;
    }
    
    if (!hasDeviceQuery) {
        g_logger.error("CefRendererGPUWin: EGL_EXT_device_query extension not found");
        return false;
    }
    
    if (!hasAngleDevice) {
        g_logger.error("CefRendererGPUWin: EGL_ANGLE_device_d3d extension not found");
        return false;
    }
    
    const char* vendor = eglQueryString(display, EGL_VENDOR);
    g_logger.info(stdext::format("CefRendererGPUWin: EGL Vendor: %s", vendor ? vendor : "unknown"));
    
    if (vendor && strstr(vendor, "Google Inc.")) {
        g_logger.info("CefRendererGPUWin: ANGLE renderer detected");
    } else {
        g_logger.warning("CefRendererGPUWin: Non-ANGLE EGL implementation detected");
    }
    
    g_logger.info("CefRendererGPUWin: GPU acceleration is supported!");
    return true;
#else
    g_logger.info("CefRendererGPUWin: GPU acceleration disabled - not compiled for Windows OpenGL ES 2.0");
    return false;
#endif
}

void CefRendererGPUWin::onRenderSupported(CefWindowInfo& windowInfo) const
{
    if (g_cefConfig && g_cefConfig->shouldUseSharedTexture() && isSupported()) {
        windowInfo.shared_texture_enabled = true;
        g_logger.info("CefRendererGPUWin: Shared texture enabled for CEF browser");
    }
}

#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2

bool CefRendererGPUWin::initializeAngleInterop()
{
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        g_logger.error("CefRendererGPUWin: No current EGL display for ANGLE interop");
        return false;
    }

    // Get extension function pointers
    eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC)eglGetProcAddress("eglQueryDisplayAttribEXT");
    eglQueryDeviceAttribEXT = (PFNEGLQUERYDEVICEATTRIBEXTPROC)eglGetProcAddress("eglQueryDeviceAttribEXT");
    
    if (!eglQueryDisplayAttribEXT || !eglQueryDeviceAttribEXT) {
        g_logger.error("CefRendererGPUWin: Failed to get EGL extension function pointers");
        return false;
    }

    // Query ANGLE's D3D11 device
    EGLDeviceEXT eglDevice = nullptr;
    if (!eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice) || !eglDevice) {
        g_logger.error("CefRendererGPUWin: Failed to query EGL device");
        return false;
    }

    if (!eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, (EGLAttrib*)&m_angleD3D11Device) || !m_angleD3D11Device) {
        g_logger.error("CefRendererGPUWin: Failed to get ANGLE D3D11 device");
        return false;
    }

    // Get D3D11.1 interface for OpenSharedResource1
    HRESULT hr = m_angleD3D11Device->QueryInterface(__uuidof(ID3D11Device1), (void**)&m_angleD3D11Device1);
    if (FAILED(hr)) {
        g_logger.warning("CefRendererGPUWin: D3D11.1 interface not available, will use legacy OpenSharedResource");
    }

    // Get device context
    m_angleD3D11Device->GetImmediateContext(&m_angleD3D11Context);

    g_logger.info("CefRendererGPUWin: ANGLE D3D11 interop initialized successfully");
    return true;
}

void CefRendererGPUWin::cleanupResources()
{
    cleanupEGLPbuffer();
    
    if (m_angleDestTexture) {
        m_angleDestTexture->Release();
        m_angleDestTexture = nullptr;
    }
    
    if (m_angleD3D11Context) {
        m_angleD3D11Context->Release();
        m_angleD3D11Context = nullptr;
    }
    
    if (m_angleD3D11Device1) {
        m_angleD3D11Device1->Release();
        m_angleD3D11Device1 = nullptr;
    }
    
    // Note: Don't release m_angleD3D11Device as it's owned by ANGLE
    m_angleD3D11Device = nullptr;
    
    m_classicSharedHandle = nullptr;
}

bool CefRendererGPUWin::createAngleDestinationTexture(int width, int height)
{
    // Clean up existing texture
    cleanupEGLPbuffer();
    if (m_angleDestTexture) {
        m_angleDestTexture->Release();
        m_angleDestTexture = nullptr;
    }

    // Create texture with classic shared handle
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA8 for ANGLE compatibility
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; // Classic shared handle

    HRESULT hr = m_angleD3D11Device->CreateTexture2D(&desc, nullptr, &m_angleDestTexture);
    if (FAILED(hr)) {
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to create destination texture, HRESULT: 0x%x", hr));
        return false;
    }

    // Get classic shared handle
    IDXGIResource* dxgiResource = nullptr;
    hr = m_angleDestTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
    if (FAILED(hr)) {
        g_logger.error("CefRendererGPUWin: Failed to query DXGI resource interface");
        return false;
    }

    hr = dxgiResource->GetSharedHandle(&m_classicSharedHandle);
    dxgiResource->Release();
    
    if (FAILED(hr) || !m_classicSharedHandle) {
        g_logger.error("CefRendererGPUWin: Failed to get classic shared handle");
        return false;
    }

    g_logger.debug(stdext::format("CefRendererGPUWin: Created destination texture %dx%d with classic handle %p", 
                                  width, height, m_classicSharedHandle));
    return true;
}

bool CefRendererGPUWin::setupEGLPbuffer()
{
    EGLDisplay display = eglGetCurrentDisplay();
    
    // Choose EGL config
    EGLint numCfg;
    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    if (!eglChooseConfig(display, cfgAttrs, &m_eglConfig, 1, &numCfg) || numCfg == 0) {
        g_logger.error("CefRendererGPUWin: Failed to choose EGL config for pbuffer");
        return false;
    }

    // Create pbuffer from classic shared handle
    EGLint attrs[] = {
        EGL_WIDTH, m_lastWidth,
        EGL_HEIGHT, m_lastHeight,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    m_pbuffer = eglCreatePbufferFromClientBuffer(
        display,
        EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        (EGLClientBuffer)m_classicSharedHandle,
        m_eglConfig,
        attrs
    );

    if (m_pbuffer == EGL_NO_SURFACE) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to create pbuffer from classic handle, EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        return false;
    }

    // Bind to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
    
    if (!eglBindTexImage(display, m_pbuffer, EGL_BACK_BUFFER)) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to bind pbuffer to texture, EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        return false;
    }

    m_pbufferBound = true;

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    g_logger.debug("CefRendererGPUWin: EGL pbuffer setup successful");
    return true;
}

void CefRendererGPUWin::cleanupEGLPbuffer()
{
    EGLDisplay display = eglGetCurrentDisplay();
    
    if (m_pbufferBound && m_pbuffer != EGL_NO_SURFACE) {
        eglReleaseTexImage(display, m_pbuffer, EGL_BACK_BUFFER);
        m_pbufferBound = false;
    }
    
    if (m_pbuffer != EGL_NO_SURFACE) {
        eglDestroySurface(display, m_pbuffer);
        m_pbuffer = EGL_NO_SURFACE;
    }
}

bool CefRendererGPUWin::copyFromCEFTexture(HANDLE ntHandle)
{
    ID3D11Texture2D* srcTexture = nullptr;
    
    // Open CEF's shared resource (try NT handle first, fallback to classic)
    if (!openSharedResourceSafely(ntHandle, &srcTexture)) {
        return false;
    }

    // Handle keyed mutex if present
    bool mutexAcquired = handleKeyedMutex(srcTexture, m_angleDestTexture, true);

    // Copy from source to destination
    m_angleD3D11Context->CopyResource(m_angleDestTexture, srcTexture);

    // Release keyed mutex if acquired
    if (mutexAcquired) {
        handleKeyedMutex(srcTexture, m_angleDestTexture, false);
    }

    srcTexture->Release();
    return true;
}

bool CefRendererGPUWin::openSharedResourceSafely(HANDLE handle, ID3D11Texture2D** outTexture)
{
    HRESULT hr = E_FAIL;
    
    // Try D3D11.1 OpenSharedResource1 first for NT handles
    if (m_angleD3D11Device1) {
        hr = m_angleD3D11Device1->OpenSharedResource1(handle, __uuidof(ID3D11Texture2D), (void**)outTexture);
        if (SUCCEEDED(hr)) {
            g_logger.debug("CefRendererGPUWin: Opened NT handle with OpenSharedResource1");
            return true;
        }
        g_logger.debug(stdext::format("CefRendererGPUWin: OpenSharedResource1 failed with 0x%x, trying legacy method", hr));
    }
    
    // Fallback to classic OpenSharedResource
    hr = m_angleD3D11Device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)outTexture);
    if (SUCCEEDED(hr)) {
        g_logger.debug("CefRendererGPUWin: Opened handle with legacy OpenSharedResource");
        return true;
    }
    
    g_logger.error(stdext::format("CefRendererGPUWin: Both OpenSharedResource methods failed, HRESULT: 0x%x", hr));
    return false;
}

bool CefRendererGPUWin::handleKeyedMutex(ID3D11Texture2D* srcTexture, ID3D11Texture2D* dstTexture, bool acquire)
{
    IDXGIKeyedMutex* srcMutex = nullptr;
    IDXGIKeyedMutex* dstMutex = nullptr;
    
    // Query for keyed mutex interfaces
    srcTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&srcMutex);
    dstTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&dstMutex);
    
    if (!srcMutex && !dstMutex) {
        return false; // No keyed mutexes present
    }
    
    if (acquire) {
        // Acquire mutexes (source first, then destination)
        if (srcMutex) {
            HRESULT hr = srcMutex->AcquireSync(0, INFINITE);
            if (FAILED(hr)) {
                g_logger.warning(stdext::format("CefRendererGPUWin: Failed to acquire source mutex: 0x%x", hr));
            }
        }
        
        if (dstMutex) {
            HRESULT hr = dstMutex->AcquireSync(0, INFINITE);
            if (FAILED(hr)) {
                g_logger.warning(stdext::format("CefRendererGPUWin: Failed to acquire destination mutex: 0x%x", hr));
            }
        }
    } else {
        // Release mutexes (destination first, then source)
        if (dstMutex) {
            dstMutex->ReleaseSync(0);
        }
        
        if (srcMutex) {
            srcMutex->ReleaseSync(0);
        }
    }
    
    if (srcMutex) srcMutex->Release();
    if (dstMutex) dstMutex->Release();
    
    return true;
}

#endif

