#include "cef_renderergpuwin.h"
#include "../../ui/uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/graphics/graphics.h>
#include "../../core/cef_init.h"
#if defined(USE_CEF)
#include <include/cef_browser.h>
#endif
#if defined(USE_CEF) && defined(_WIN32)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <d3d11.h>
#include <dxgi.h>
#include <GL/gl.h>
#endif

CefRendererGPUWin::CefRendererGPUWin(UICEFWebView& view)
    : CefRenderer(view)
    , m_lastWidth(0)
    , m_lastHeight(0)
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    , m_d3dDevice(nullptr)
    , m_d3dContext(nullptr)
    , m_angleExtensionsChecked(false)
    , m_hasD3D11Extension(false)
    , m_hasImageBaseExtension(false)
    , eglCreateImageKHR(nullptr)
    , eglDestroyImageKHR(nullptr)
    , glEGLImageTargetTexture2DOES(nullptr)
#endif
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    // Log system information for debugging
    logD3D11DeviceInfo();
    logEGLInfo();
    
    if (!initializeD3DDevice()) {
        g_logger.error("CefRendererGPUWin: Failed to initialize D3D11 device");
    }
    if (!checkAngleExtensions()) {
        g_logger.error("CefRendererGPUWin: Required ANGLE extensions not available");
    }
#endif
}

CefRendererGPUWin::~CefRendererGPUWin()
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    cleanupD3DResources();
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
    if (!m_d3dDevice || !m_hasD3D11Extension || !m_hasImageBaseExtension) {
        g_logger.error("CefRendererGPUWin: Required D3D11 device or ANGLE extensions not available");
        return;
    }

    HANDLE sharedHandle = static_cast<HANDLE>(info.shared_texture_handle);
    if (!sharedHandle || sharedHandle == INVALID_HANDLE_VALUE) {
        g_logger.error("CefRendererGPUWin: Invalid shared texture handle");
        return;
    }

    g_logger.debug(stdext::format("CefRendererGPUWin: Processing shared handle %p, size: %dx%d", 
                                  sharedHandle, info.extra.coded_size.width, info.extra.coded_size.height));

    if (!createTextureFromSharedHandle(sharedHandle, info)) {
        g_logger.error("CefRendererGPUWin: Failed to create texture from shared handle");
    }
#else
    (void)info;
#endif
}

bool CefRendererGPUWin::isSupported() const
{
#if defined(USE_CEF) && defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    return m_d3dDevice != nullptr && m_hasD3D11Extension && m_hasImageBaseExtension;
#else
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

bool CefRendererGPUWin::initializeD3DDevice()
{
    if (m_d3dDevice) {
        return true; // Already initialized
    }

    // Create D3D11 device with proper flags for shared resources
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware acceleration
        nullptr,                    // No software module
        createDeviceFlags,          // Device flags
        featureLevels,              // Feature levels to try
        ARRAYSIZE(featureLevels),   // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &m_d3dDevice,               // Device output
        &featureLevel,              // Actual feature level
        &m_d3dContext               // Device context output
    );

    if (FAILED(hr)) {
        g_logger.error(stdext::format("CefRendererGPUWin: D3D11CreateDevice failed with HRESULT 0x%x", hr));
        return false;
    }

    g_logger.info(stdext::format("CefRendererGPUWin: D3D11 device created successfully, feature level: 0x%x", featureLevel));

    // Verify the device supports the required functionality
    ID3D11Device1* device1 = nullptr;
    hr = m_d3dDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&device1);
    if (SUCCEEDED(hr)) {
        g_logger.info("CefRendererGPUWin: D3D11.1 interface available");
        device1->Release();
    }

    return true;
}

void CefRendererGPUWin::cleanupD3DResources()
{
    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }
}

bool CefRendererGPUWin::checkAngleExtensions()
{
    if (m_angleExtensionsChecked) {
        return m_hasD3D11Extension && m_hasImageBaseExtension;
    }

    m_angleExtensionsChecked = true;

    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        g_logger.error("CefRendererGPUWin: No current EGL display");
        return false;
    }

    // Check for required extensions
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (!extensions) {
        g_logger.error("CefRendererGPUWin: Failed to query EGL extensions");
        return false;
    }

    g_logger.info(stdext::format("CefRendererGPUWin: EGL extensions: %s", extensions));

    // Check for D3D11 texture sharing extension (ANGLE-specific)
    m_hasD3D11Extension = strstr(extensions, "EGL_ANGLE_d3d_texture_client_buffer") != nullptr;
    
    // Check for EGL image extensions
    m_hasImageBaseExtension = strstr(extensions, "EGL_KHR_image_base") != nullptr ||
                              strstr(extensions, "EGL_KHR_image") != nullptr;

    if (!m_hasD3D11Extension) {
        g_logger.error("CefRendererGPUWin: EGL_ANGLE_d3d_texture_client_buffer extension not available");
    }

    if (!m_hasImageBaseExtension) {
        g_logger.error("CefRendererGPUWin: EGL_KHR_image_base extension not available");
    }

    // Get function pointers for EGL image functions
    if (m_hasImageBaseExtension) {
        eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

        if (!eglCreateImageKHR || !eglDestroyImageKHR || !glEGLImageTargetTexture2DOES) {
            g_logger.error("CefRendererGPUWin: Failed to get EGL image function pointers");
            m_hasImageBaseExtension = false;
        }
    }

    g_logger.info(stdext::format("CefRendererGPUWin: Extension check - D3D11: %s, EGL Image: %s", 
                                m_hasD3D11Extension ? "available" : "missing",
                                m_hasImageBaseExtension ? "available" : "missing"));

    return m_hasD3D11Extension && m_hasImageBaseExtension;
}

bool CefRendererGPUWin::createTextureFromSharedHandle(HANDLE sharedHandle, const CefAcceleratedPaintInfo& info)
{
    // Open the shared D3D11 texture
    ID3D11Texture2D* sharedTexture = nullptr;
    HRESULT hr = m_d3dDevice->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)&sharedTexture);
    
    if (FAILED(hr)) {
        g_logger.error(stdext::format("CefRendererGPUWin: OpenSharedResource failed with HRESULT 0x%x", hr));
        
        // Additional diagnostic information
        switch (hr) {
            case E_INVALIDARG:
                g_logger.error("CefRendererGPUWin: Invalid argument - handle may be invalid or incompatible");
                break;
            case E_OUTOFMEMORY:
                g_logger.error("CefRendererGPUWin: Out of memory");
                break;
            case DXGI_ERROR_INVALID_CALL:
                g_logger.error("CefRendererGPUWin: Invalid call - device may not support shared resources");
                break;
            default:
                g_logger.error(stdext::format("CefRendererGPUWin: Unknown D3D11 error: 0x%x", hr));
                break;
        }
        return false;
    }

    // Get texture description for validation
    D3D11_TEXTURE2D_DESC textureDesc;
    sharedTexture->GetDesc(&textureDesc);
    
    g_logger.debug(stdext::format("CefRendererGPUWin: Opened shared texture - Size: %dx%d, Format: %d, Usage: %d", 
                                  textureDesc.Width, textureDesc.Height, textureDesc.Format, textureDesc.Usage));

    // Create or update OpenGL texture if needed
    if (!m_cefTexture || m_lastWidth != info.extra.coded_size.width || m_lastHeight != info.extra.coded_size.height) {
        m_cefTexture = TexturePtr(new Texture(Size(info.extra.coded_size.width, info.extra.coded_size.height)));
        m_lastWidth = info.extra.coded_size.width;
        m_lastHeight = info.extra.coded_size.height;
        m_textureCreated = true;
        
        g_logger.info(stdext::format("CefRendererGPUWin: Created new OpenGL texture - Size: %dx%d", 
                                     m_lastWidth, m_lastHeight));
    }

    EGLDisplay display = eglGetCurrentDisplay();
    
    // Create EGL image from D3D11 texture using ANGLE extension
    EGLint imageAttribs[] = {
        EGL_NONE
    };

    EGLImageKHR eglImage = eglCreateImageKHR(
        display,
        EGL_NO_CONTEXT,
        EGL_D3D11_TEXTURE_2D_ANGLE,
        (EGLClientBuffer)sharedTexture,
        imageAttribs
    );

    if (eglImage == EGL_NO_IMAGE_KHR) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("CefRendererGPUWin: eglCreateImageKHR failed - EGL error: 0x%x (%s)",
                                      eglError, getEGLErrorString(eglError)));
        sharedTexture->Release();
        return false;
    }

    // Bind the EGL image to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
    
    // Check for OpenGL errors
    GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        g_logger.error(stdext::format("CefRendererGPUWin: glEGLImageTargetTexture2DOES failed - GL error: 0x%x", glError));
        eglDestroyImageKHR(display, eglImage);
        sharedTexture->Release();
        return false;
    }

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Cleanup
    eglDestroyImageKHR(display, eglImage);
    sharedTexture->Release();

    g_logger.debug("CefRendererGPUWin: Successfully created texture from shared D3D11 resource");
    return true;
}

#endif

