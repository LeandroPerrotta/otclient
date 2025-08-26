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
    , m_d3d11Device(nullptr)
    , m_d3d11Device1(nullptr)
    , m_d3d11Context(nullptr)
    , m_destTexture(nullptr)
    , m_classicSharedHandle(nullptr)
    , m_pbuffer(EGL_NO_SURFACE)
    , m_pbufferBound(false)
#endif
{
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

    // Duplicate the handle for cross-thread usage
    HANDLE duplicatedHandle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), ntHandle, GetCurrentProcess(), &duplicatedHandle, 
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        DWORD error = GetLastError();
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to duplicate handle, error: 0x%x", error));
        return;
    }

    g_logger.debug(stdext::format("CefRendererGPUWin: Processing NT handle %p (duplicated: %p), size: %dx%d", 
                                  ntHandle, duplicatedHandle, width, height));

    // Move operations to main thread where OpenGL context lives
    g_dispatcher.addEventFromOtherThread([this, duplicatedHandle, width, height]() mutable {
        auto closeHandle = [](HANDLE& h) { if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = nullptr; } };
        
        // First, ensure we have a D3D11 device by trying to open the CEF texture
        if (!m_d3d11Device) {
            ID3D11Texture2D* tempTexture = nullptr;
            LUID adapterLuid = {};
            
            // Open CEF texture to find which adapter it's on
            if (!openSharedResourceSafely(duplicatedHandle, &tempTexture, &adapterLuid)) {
                g_logger.error("CefRendererGPUWin: Failed to open CEF texture to determine adapter");
                closeHandle(duplicatedHandle);
                return;
            }
            
            tempTexture->Release(); // We only needed this to find the adapter
            
            // Create device on the same adapter as CEF
            if (!createDeviceOnAdapter(adapterLuid)) {
                g_logger.error("CefRendererGPUWin: Failed to create device on CEF adapter");
                closeHandle(duplicatedHandle);
                return;
            }
        }

        // Check if we need to recreate destination texture for new size
        if (!m_destTexture || m_lastWidth != width || m_lastHeight != height) {
            if (!createDestinationTexture(width, height)) {
                g_logger.error("CefRendererGPUWin: Failed to create destination texture");
                closeHandle(duplicatedHandle);
                return;
            }
            
            if (!setupEGLPbuffer()) {
                g_logger.error("CefRendererGPUWin: Failed to setup EGL pbuffer");
                closeHandle(duplicatedHandle);
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
        if (!copyFromCEFTexture(duplicatedHandle)) {
            g_logger.error("CefRendererGPUWin: Failed to copy from CEF texture");
            closeHandle(duplicatedHandle);
            return;
        }

        // Clean up the duplicated handle
        closeHandle(duplicatedHandle);
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
    
    // Check for required extension
    bool hasD3DExtension = strstr(extensions, "EGL_ANGLE_d3d_share_handle_client_buffer") != nullptr;
    
    if (!hasD3DExtension) {
        g_logger.error("CefRendererGPUWin: EGL_ANGLE_d3d_share_handle_client_buffer extension not found");
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

bool CefRendererGPUWin::initializeD3D11Device()
{
    // We'll create the device on-demand when we get the first CEF texture
    // and can determine which adapter it's on
    g_logger.info("CefRendererGPUWin: D3D11 device will be created on-demand based on CEF texture adapter");
    return true;
}

void CefRendererGPUWin::cleanupResources()
{
    cleanupEGLPbuffer();
    
    if (m_destTexture) {
        m_destTexture->Release();
        m_destTexture = nullptr;
    }
    
    if (m_d3d11Context) {
        m_d3d11Context->Release();
        m_d3d11Context = nullptr;
    }
    
    if (m_d3d11Device1) {
        m_d3d11Device1->Release();
        m_d3d11Device1 = nullptr;
    }
    
    if (m_d3d11Device) {
        m_d3d11Device->Release();
        m_d3d11Device = nullptr;
    }
    
    m_classicSharedHandle = nullptr;
}

bool CefRendererGPUWin::createDestinationTexture(int width, int height)
{
    // Clean up existing texture
    cleanupEGLPbuffer();
    if (m_destTexture) {
        m_destTexture->Release();
        m_destTexture = nullptr;
    }

    if (!m_d3d11Device) {
        g_logger.error("CefRendererGPUWin: No D3D11 device available for texture creation");
        return false;
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

    HRESULT hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, &m_destTexture);
    if (FAILED(hr)) {
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to create destination texture, HRESULT: 0x%x", hr));
        return false;
    }

    // Get classic shared handle
    IDXGIResource* dxgiResource = nullptr;
    hr = m_destTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
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
    
    // Open CEF's shared resource with our existing device
    if (!openSharedResourceSafely(ntHandle, &srcTexture)) {
        return false;
    }

    // Handle keyed mutex if present
    bool mutexAcquired = handleKeyedMutex(srcTexture, m_destTexture, true);

    // Copy from source to destination
    m_d3d11Context->CopyResource(m_destTexture, srcTexture);

    // Release keyed mutex if acquired
    if (mutexAcquired) {
        handleKeyedMutex(srcTexture, m_destTexture, false);
    }

    srcTexture->Release();
    return true;
}

bool CefRendererGPUWin::openSharedResourceSafely(HANDLE handle, ID3D11Texture2D** outTexture, LUID* adapterLuid)
{
    HRESULT hr = E_FAIL;
    ID3D11Device* deviceToUse = m_d3d11Device;
    ID3D11Device1* device1ToUse = m_d3d11Device1;
    
    // If we need to find the adapter, create a temporary device
    ID3D11Device* tempDevice = nullptr;
    ID3D11Device1* tempDevice1 = nullptr;
    ID3D11DeviceContext* tempContext = nullptr;
    
    if (!deviceToUse && adapterLuid) {
        // Create temporary device to find adapter
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        
        D3D_FEATURE_LEVEL featureLevel;
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &tempDevice, &featureLevel, &tempContext
        );
        
        if (FAILED(hr)) {
            g_logger.error(stdext::format("CefRendererGPUWin: Failed to create temporary D3D11 device: 0x%x", hr));
            return false;
        }
        
        tempDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&tempDevice1);
        deviceToUse = tempDevice;
        device1ToUse = tempDevice1;
    }
    
    // Try D3D11.1 OpenSharedResource1 first for NT handles
    if (device1ToUse) {
        hr = device1ToUse->OpenSharedResource1(handle, __uuidof(ID3D11Texture2D), (void**)outTexture);
        if (SUCCEEDED(hr)) {
            g_logger.debug("CefRendererGPUWin: Opened NT handle with OpenSharedResource1");
            
            // Get adapter LUID if requested
            if (adapterLuid && *outTexture) {
                IDXGIDevice* dxgiDevice = nullptr;
                if (SUCCEEDED(deviceToUse->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
                    IDXGIAdapter* adapter = nullptr;
                    if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                        DXGI_ADAPTER_DESC desc;
                        if (SUCCEEDED(adapter->GetDesc(&desc))) {
                            *adapterLuid = desc.AdapterLuid;
                            g_logger.debug(stdext::format("CefRendererGPUWin: Found adapter LUID: %08x-%08x", 
                                                         adapterLuid->HighPart, adapterLuid->LowPart));
                        }
                        adapter->Release();
                    }
                    dxgiDevice->Release();
                }
            }
            
            // Cleanup temp resources
            if (tempContext) tempContext->Release();
            if (tempDevice1) tempDevice1->Release();
            if (tempDevice) tempDevice->Release();
            return true;
        }
        g_logger.debug(stdext::format("CefRendererGPUWin: OpenSharedResource1 failed with 0x%x, trying legacy method", hr));
    }
    
    // Fallback to classic OpenSharedResource
    if (deviceToUse) {
        hr = deviceToUse->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)outTexture);
        if (SUCCEEDED(hr)) {
            g_logger.debug("CefRendererGPUWin: Opened handle with legacy OpenSharedResource");
            
            // Cleanup temp resources
            if (tempContext) tempContext->Release();
            if (tempDevice1) tempDevice1->Release();
            if (tempDevice) tempDevice->Release();
            return true;
        }
    }
    
    // Cleanup temp resources on failure
    if (tempContext) tempContext->Release();
    if (tempDevice1) tempDevice1->Release();
    if (tempDevice) tempDevice->Release();
    
    g_logger.error(stdext::format("CefRendererGPUWin: Both OpenSharedResource methods failed, HRESULT: 0x%x", hr));
    return false;
}

bool CefRendererGPUWin::createDeviceOnAdapter(const LUID& adapterLuid)
{
    // Enumerate adapters to find the one with matching LUID
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr)) {
        g_logger.error("CefRendererGPUWin: Failed to create DXGI factory");
        return false;
    }

    IDXGIAdapter1* targetAdapter = nullptr;
    for (UINT i = 0; ; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        hr = factory->EnumAdapters1(i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break; // No more adapters
        }
        if (FAILED(hr)) {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            if (desc.AdapterLuid.LowPart == adapterLuid.LowPart && 
                desc.AdapterLuid.HighPart == adapterLuid.HighPart) {
                targetAdapter = adapter;
                g_logger.info(stdext::format("CefRendererGPUWin: Found matching adapter: %ws", desc.Description));
                break;
            }
        }
        adapter->Release();
    }
    factory->Release();

    if (!targetAdapter) {
        g_logger.error("CefRendererGPUWin: Could not find adapter with matching LUID");
        return false;
    }

    // Create D3D11 device on the target adapter
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        targetAdapter,
        D3D_DRIVER_TYPE_UNKNOWN, // Must use UNKNOWN when specifying adapter
        nullptr,
        0,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_d3d11Device,
        &featureLevel,
        &m_d3d11Context
    );

    targetAdapter->Release();

    if (FAILED(hr)) {
        g_logger.error(stdext::format("CefRendererGPUWin: Failed to create D3D11 device on adapter, HRESULT: 0x%x", hr));
        return false;
    }

    // Get D3D11.1 interface
    m_d3d11Device->QueryInterface(__uuidof(ID3D11Device1), (void**)&m_d3d11Device1);

    g_logger.info(stdext::format("CefRendererGPUWin: Created D3D11 device on CEF adapter, feature level: 0x%x", featureLevel));
    return true;
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

