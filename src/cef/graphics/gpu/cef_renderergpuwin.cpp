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

void CefRendererGPUWin::onAcceleratedPaint(const CefAcceleratedPaintInfo& info, const CefRenderHandler::RectList* dirtyRects)
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

        // Remove per-frame debug log

            // Copy dirty rects for use in the lambda (empty if null)
        CefRenderHandler::RectList rectsCopy = dirtyRects ? *dirtyRects : CefRenderHandler::RectList();
        
        // Move operations to main thread where OpenGL context lives  
        g_dispatcher.addEventFromOtherThread([this, duplicatedHandle, width, height, rectsCopy]() mutable {
        auto closeHandle = [](HANDLE& h) { if (h && h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = nullptr; } };
        
        // Save current EGL context state for restoration
        EGLDisplay currentDisplay = eglGetCurrentDisplay();
        EGLContext currentContext = eglGetCurrentContext();
        EGLSurface currentDrawSurface = eglGetCurrentSurface(EGL_DRAW);
        EGLSurface currentReadSurface = eglGetCurrentSurface(EGL_READ);
        
        // Verify we have a valid EGL context
        if (currentDisplay == EGL_NO_DISPLAY || currentContext == EGL_NO_CONTEXT) {
            g_logger.error("CefRendererGPUWin: No valid EGL context available for accelerated paint");
            closeHandle(duplicatedHandle);
            return;
        }
        
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
            
            // Create OpenGL texture
            m_cefTexture = TexturePtr(new Texture(Size(width, height)));
            m_lastWidth = width;
            m_lastHeight = height;
            m_textureCreated = true;

            if (!setupEGLPbuffer()) {
                g_logger.error("CefRendererGPUWin: Failed to setup EGL pbuffer");
                closeHandle(duplicatedHandle);
                return;
            }
            
            g_logger.info(stdext::format("CefRendererGPUWin: Created new texture %dx%d", width, height));
        }

                    // Copy from CEF's NT handle to our classic handle texture
            if (!copyFromCEFTexture(duplicatedHandle, rectsCopy)) {
                g_logger.error("CefRendererGPUWin: Failed to copy from CEF texture");
                closeHandle(duplicatedHandle);
                return;
            }

        // After D3D11 copy, we may need to refresh the EGL binding
        // This ensures the OpenGL texture sees the updated content
        if (m_pbufferBound) {
            EGLDisplay display = eglGetCurrentDisplay();
            
            // Release and rebind the texture to refresh content
            glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
            eglReleaseTexImage(display, m_pbuffer, EGL_BACK_BUFFER);
            
            if (!eglBindTexImage(display, m_pbuffer, EGL_BACK_BUFFER)) {
                EGLint eglError = eglGetError();
                g_logger.warning(stdext::format("CefRendererGPUWin: Failed to refresh texture binding, EGL error: 0x%x", eglError));
            }
            // Texture refresh completed (removed success log)
        }

        // Restore original EGL context if it was different
        EGLDisplay finalDisplay = eglGetCurrentDisplay();
        EGLContext finalContext = eglGetCurrentContext();
        
        if (finalDisplay != currentDisplay || finalContext != currentContext) {
            if (!eglMakeCurrent(currentDisplay, currentDrawSurface, currentReadSurface, currentContext)) {
                EGLint eglError = eglGetError();
                g_logger.warning(stdext::format("CefRendererGPUWin: Failed to restore original EGL context, error: 0x%x", eglError));
            }
        }
        
        // Clean up the duplicated handle
        closeHandle(duplicatedHandle);
        // Frame processed successfully (removed per-frame log)
    });
#else
    (void)info;
    (void)dirtyRects;
#endif
}

bool CefRendererGPUWin::isSupported() const
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    if(g_cefConfig && !g_cefConfig->shouldUseSharedTexture()) {
        g_logger.info("CefRendererGPUWin: Shared texture disabled by config");
        return false;
    }

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    g_logger.info(stdext::format("GL_RENDERER: %s", renderer ? renderer : "null"));    
    
    // Check for Intel graphics and apply specific compatibility settings
    if (renderer && strstr(renderer, "Intel")) {
        g_logger.info(stdext::format("CefRendererGPUWin: Detected Intel graphics (%s) - applying Intel-specific compatibility settings", renderer));
        
        // Intel graphics detected - we'll continue with GPU acceleration but with specific settings
        // The actual compatibility fixes will be applied in the renderer setup
    }
    
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
    
    g_logger.info("CefRendererGPUWin: Supported");
    return true;
#else
    g_logger.info("CefRendererGPUWin: GPU acceleration disabled - not compiled for Windows OpenGL ES 2.0");
    return false;
#endif
}

void CefRendererGPUWin::onRenderSupported(CefWindowInfo& windowInfo) const
{
    if (isSupported()) {
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

    // Intel graphics prefer simpler texture configurations
    struct TextureConfig {
        DXGI_FORMAT format;
        UINT bindFlags;
        const char* name;
    } configs[] = {
        // For Intel graphics, try simpler configurations first
        isIntelGraphics() ? 
            // Intel-optimized order
            (TextureConfig{ DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, "BGRA8 Simple (Intel)" }) :
            // Standard order for other GPUs
            (TextureConfig{ DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, "BGRA8 Renderable" }),
        
        { DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, "RGBA8 Renderable" },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, "BGRA8 Renderable" },
        
        // Fallback with additional flags (only for non-Intel)
        { DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, "RGBA8 Full Access" },
        { DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS, "BGRA8 Full Access" },
    };

    int maxConfigs = isIntelGraphics() ? 3 : 5; // Skip complex configs for Intel
    for (int i = 0; i < maxConfigs; i++) {
        // Create texture with classic shared handle
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = configs[i].format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = configs[i].bindFlags;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; // Classic shared handle

        HRESULT hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, &m_destTexture);
        if (FAILED(hr)) {
                        g_logger.info(stdext::format("CefRendererGPUWin: Failed to create texture (%s), HRESULT: 0x%x", 
                                          configs[i].name, hr));
            continue;
        }

        // Get classic shared handle
        IDXGIResource* dxgiResource = nullptr;
        hr = m_destTexture->QueryInterface(__uuidof(IDXGIResource), (void**)&dxgiResource);
        if (FAILED(hr)) {
            g_logger.info(stdext::format("CefRendererGPUWin: Failed to query DXGI resource interface (%s)", configs[i].name));
            m_destTexture->Release();
            m_destTexture = nullptr;
            continue;
        }

        hr = dxgiResource->GetSharedHandle(&m_classicSharedHandle);
        dxgiResource->Release();
        
        if (FAILED(hr) || !m_classicSharedHandle) {
            g_logger.info(stdext::format("CefRendererGPUWin: Failed to get classic shared handle (%s)", configs[i].name));
            m_destTexture->Release();
            m_destTexture = nullptr;
            continue;
        }

        // Success!
        g_logger.info(stdext::format("CefRendererGPUWin: Created destination texture %dx%d (%s) with handle %p", 
                                     width, height, configs[i].name, m_classicSharedHandle));
        return true;
    }

    g_logger.error("CefRendererGPUWin: Failed to create destination texture with any configuration");
    return false;
}

bool CefRendererGPUWin::setupEGLPbuffer()
{
    // Save current EGL context state for restoration
    EGLDisplay originalDisplay = eglGetCurrentDisplay();
    EGLContext originalContext = eglGetCurrentContext();
    EGLSurface originalDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface originalReadSurface = eglGetCurrentSurface(EGL_READ);
    
    EGLDisplay display = originalDisplay;
    EGLContext context = originalContext;
    EGLSurface currentSurface = originalDrawSurface;
    
    g_logger.info(stdext::format("CefRendererGPUWin: EGL state - Display: %p, Context: %p, Surface: %p", 
                                 display, context, currentSurface));
    
    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT) {
        g_logger.error("CefRendererGPUWin: Invalid EGL state for pbuffer creation");
        return false;
    }
    
    // Try configs with bind-to-texture support first, then fallback to basic configs
    struct ConfigAttempt {
        const char* name;
        EGLint attrs[20];
        bool expectsBinding;
    } attempts[] = {
        {
            "BGRA8 with bind to texture",
            {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
                EGL_NONE
            },
            true
        },
        {
            "Basic RGBA8 config (no binding)",
            {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_NONE
            },
            false
        }
    };
    
    bool configFound = false;
    bool configSupportsBinding = false;
    const char* usedConfigName = nullptr;
    
    for (int configAttempt = 0; configAttempt < 2 && !configFound; configAttempt++) {
        EGLint numCfg;
        if (!eglChooseConfig(display, attempts[configAttempt].attrs, &m_eglConfig, 1, &numCfg) || numCfg == 0) {
            g_logger.info(stdext::format("CefRendererGPUWin: Config attempt '%s' failed", attempts[configAttempt].name));
            continue;
        }
        
        configFound = true;
        configSupportsBinding = attempts[configAttempt].expectsBinding;
        usedConfigName = attempts[configAttempt].name;
        // Using selected EGL config (removed per-setup log)
    }
    
    if (!configFound) {
        g_logger.error("CefRendererGPUWin: Failed to find any suitable EGL config");
        return false;
    }

    // Verify the actual bind-to-texture support
    EGLint bindToTextureRGBA = 0;
    eglGetConfigAttrib(display, m_eglConfig, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
    // Config verified (removed detailed config log)

    // Create pbuffer with texture binding attributes
    
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
        g_logger.error(stdext::format("CefRendererGPUWin: Pbuffer creation failed - EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        return false;
    }

    g_logger.info("CefRendererGPUWin: Pbuffer created successfully");

    // Check if we have a valid OpenGL texture
    if (!m_cefTexture) {
        g_logger.error("CefRendererGPUWin: No OpenGL texture available");
        return false;
    }

    // Handle texture binding based on config capability
    if (bindToTextureRGBA) {
        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        
        if (eglBindTexImage(display, m_pbuffer, EGL_BACK_BUFFER)) {
            m_pbufferBound = true;
            
            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            // Restore original EGL context before success return
            EGLDisplay finalDisplay = eglGetCurrentDisplay();
            EGLContext finalContext = eglGetCurrentContext();
            
            if (finalDisplay != originalDisplay || finalContext != originalContext) {
                if (!eglMakeCurrent(originalDisplay, originalDrawSurface, originalReadSurface, originalContext)) {
                    EGLint eglError = eglGetError();
                    g_logger.warning(stdext::format("CefRendererGPUWin: Failed to restore original EGL context after successful binding, error: 0x%x", eglError));
                }
            }
            
            g_logger.info("CefRendererGPUWin: Direct texture binding successful!");
            return true;
        } else {
            EGLint eglError = eglGetError();
            g_logger.warning(stdext::format("CefRendererGPUWin: Direct binding failed (EGL error: 0x%x), trying copy approach", eglError));
        }
    }

    // Fallback: Copy from pbuffer to texture using glReadPixels
    // We'll implement the copy in the frame update logic
    // For now, just mark that we have a working pbuffer but no direct binding
    m_pbufferBound = false;  // Not directly bound, but available for copying
    
    // Restore original EGL context
    EGLDisplay finalDisplay = eglGetCurrentDisplay();
    EGLContext finalContext = eglGetCurrentContext();
    
    if (finalDisplay != originalDisplay || finalContext != originalContext) {
        if (!eglMakeCurrent(originalDisplay, originalDrawSurface, originalReadSurface, originalContext)) {
            EGLint eglError = eglGetError();
            g_logger.warning(stdext::format("CefRendererGPUWin: Failed to restore original EGL context after pbuffer setup, error: 0x%x", eglError));
        }
    }
    
    g_logger.info("CefRendererGPUWin: Pbuffer setup completed with copy-based approach");
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

bool CefRendererGPUWin::copyFromCEFTexture(HANDLE ntHandle, const CefRenderHandler::RectList& dirtyRects)
{
    ID3D11Texture2D* srcTexture = nullptr;
    
    // Removed per-frame dirty rects log
    
    // Open CEF's shared resource with our existing device
    if (!openSharedResourceSafely(ntHandle, &srcTexture)) {
        g_logger.error("CefRendererGPUWin: Failed to open CEF shared texture");
        return false;
    }

    // Handle keyed mutex if present
    bool mutexAcquired = handleKeyedMutex(srcTexture, m_destTexture, true);
    // Removed per-frame mutex log

    // Perform copy operation
    if (dirtyRects.empty()) {
        // No dirty rects provided - copy entire texture (full copy mode)
        m_d3d11Context->CopyResource(m_destTexture, srcTexture);
    } else {
        // Copy only dirty regions for better performance
        size_t totalPixelsCopied = 0;
        for (const auto& rect : dirtyRects) {
            // Validate rect bounds
            if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
                rect.x + rect.width > m_lastWidth || rect.y + rect.height > m_lastHeight) {
                g_logger.warning(stdext::format("CefRendererGPUWin: Skipping invalid dirty rect: (%d,%d) %dx%d (texture: %dx%d)",
                                               rect.x, rect.y, rect.width, rect.height, m_lastWidth, m_lastHeight));
                continue;
            }

            // Create D3D11 box for the dirty region
            D3D11_BOX srcBox = {
                static_cast<UINT>(rect.x),                    // left
                static_cast<UINT>(rect.y),                    // top  
                0,                                            // front
                static_cast<UINT>(rect.x + rect.width),       // right
                static_cast<UINT>(rect.y + rect.height),      // bottom
                1                                             // back
            };
            
            // Copy this specific region
            m_d3d11Context->CopySubresourceRegion(
                m_destTexture, 0,                             // dest texture, subresource
                static_cast<UINT>(rect.x),                    // dest x
                static_cast<UINT>(rect.y),                    // dest y
                0,                                            // dest z
                srcTexture, 0,                                // src texture, subresource
                &srcBox                                       // src region
            );
            
            totalPixelsCopied += rect.width * rect.height;
        }
        
        // Optimization tracking removed to avoid per-frame spam
        // (Performance metrics can be added back for debugging if needed)
    }
    
    // Flush to ensure copy completes
    m_d3d11Context->Flush();

    // Release keyed mutex if acquired
    if (mutexAcquired) {
        handleKeyedMutex(srcTexture, m_destTexture, false);
    }

    srcTexture->Release();
    // D3D11 copy completed (removed per-frame log)
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
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // ANGLE needs this for texture sharing
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
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
            // Opened NT handle successfully (removed per-frame log)
            
            // Get adapter LUID if requested
            if (adapterLuid && *outTexture) {
                IDXGIDevice* dxgiDevice = nullptr;
                if (SUCCEEDED(deviceToUse->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
                    IDXGIAdapter* adapter = nullptr;
                    if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                        DXGI_ADAPTER_DESC desc;
                        if (SUCCEEDED(adapter->GetDesc(&desc))) {
                            *adapterLuid = desc.AdapterLuid;
                                                        // Found adapter LUID (removed per-frame log)
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
        // Falling back to legacy method (removed per-frame log)
    }
    
    // Fallback to classic OpenSharedResource
    if (deviceToUse) {
        hr = deviceToUse->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)outTexture);
        if (SUCCEEDED(hr)) {
            // Opened handle with legacy method (removed per-frame log)
            
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
    // Keep BGRA_SUPPORT flag as ANGLE expects it for texture sharing
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL featureLevel;
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // ANGLE needs this for proper texture sharing
    
    // Intel graphics specific flags
    if (isIntelGraphics()) {
        // For Intel graphics, we might need additional compatibility flags
        // Keep single-threaded to avoid Intel driver issues
        createFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;
        g_logger.info("CefRendererGPUWin: Using single-threaded mode for Intel graphics compatibility");
    }
    
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        targetAdapter,
        D3D_DRIVER_TYPE_UNKNOWN, // Must use UNKNOWN when specifying adapter
        nullptr,
        createFlags,  // Keep D3D11_CREATE_DEVICE_BGRA_SUPPORT
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

    g_logger.info(stdext::format("CefRendererGPUWin: Created D3D11 device on CEF adapter (with BGRA support), feature level: 0x%x", featureLevel));
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
        // For Intel graphics, use shorter timeout to prevent deadlocks
        DWORD timeout = isIntelGraphics() ? 100 : INFINITE;
        
        // Acquire mutexes (source first, then destination)
        if (srcMutex) {
            HRESULT hr = srcMutex->AcquireSync(0, timeout);
            if (FAILED(hr)) {
                if (isIntelGraphics() && hr == WAIT_TIMEOUT) {
                    g_logger.warning("CefRendererGPUWin: Intel graphics mutex timeout - skipping frame to prevent deadlock");
                } else {
                    g_logger.warning(stdext::format("CefRendererGPUWin: Failed to acquire source mutex: 0x%x", hr));
                }
            }
        }
        
        if (dstMutex) {
            HRESULT hr = dstMutex->AcquireSync(0, timeout);
            if (FAILED(hr)) {
                if (isIntelGraphics() && hr == WAIT_TIMEOUT) {
                    g_logger.warning("CefRendererGPUWin: Intel graphics mutex timeout - skipping frame to prevent deadlock");
                } else {
                    g_logger.warning(stdext::format("CefRendererGPUWin: Failed to acquire destination mutex: 0x%x", hr));
                }
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

bool CefRendererGPUWin::isIntelGraphics() const
{
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return (renderer && strstr(renderer, "Intel"));
}

void CefRendererGPUWin::applyIntelCompatibilitySettings()
{
    if (!isIntelGraphics()) {
        return;
    }
    
    g_logger.info("CefRendererGPUWin: Applying Intel graphics compatibility settings");
    
    // Intel-specific optimizations can be added here
    // For now, we mainly rely on the timeout changes in mutex handling
    // and the device creation flags that already include BGRA support
}

#endif

