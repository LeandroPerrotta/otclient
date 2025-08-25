/*
 * Copyright (c) 2010-2020 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "uicefwebview.h"
#include <framework/core/logger.h>
#include <framework/core/clock.h>
#include <framework/core/graphicalapplication.h>
#include <framework/core/eventdispatcher.h>
#include <framework/graphics/graphics.h>
#include <framework/graphics/image.h>
#include "cef_client.h"
#include "cef_inputhandler.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <memory>

#ifdef USE_CEF
#include <include/cef_app.h>
#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/cef_browser.h>
#include <include/cef_request_handler.h>
#include <include/cef_resource_request_handler.h>
#include <include/cef_life_span_handler.h>
#include <include/wrapper/cef_message_router.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_frame.h>
#include "include/cef_parser.h"
#include <GL/gl.h>
#if defined(__linux__)
#  define EGL_EGLEXT_PROTOTYPES
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
#  include <GL/glx.h>
#  include <X11/Xlib.h>
#  include <drm/drm_fourcc.h>
#  include <fcntl.h>
// Undef X11 macros that conflict with CEF
#  ifdef Success
#    undef Success
#  endif
#  ifdef None
#    undef None
#  endif
// Define X11 constants after undef
#  define X11_None 0L

// OpenGL memory object extension defines
#ifndef GL_EXT_memory_object_fd
#define GL_EXT_memory_object_fd 1
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT      0x9586
typedef void (APIENTRYP PFNGLCREATEMEMORYOBJECTSEXTPROC) (GLsizei n, GLuint *memoryObjects);
typedef void (APIENTRYP PFNGLIMPORTMEMORYFDEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM2DEXTPROC) (GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLDELETEMEMORYOBJECTSEXTPROC) (GLsizei n, const GLuint *memoryObjects);
#endif

// glEGLImageTargetTexture2DOES extension
#ifndef GLeglImageOES
typedef void* GLeglImageOES;
#endif
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
#elif defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
#  include <GLES2/gl2ext.h>
#  include <d3d11.h>
#  include <dxgi.h>
#  ifndef EGL_D3D11_TEXTURE_2D_SHARE_HANDLE_ANGLE
#  define EGL_D3D11_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x33A0
#  endif
#endif
#include <cef/resources/cefphysfsresourcehandler.h>
#include <thread>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>

// typedef (geralmente já vem de gl2ext.h, mas garantimos)
#ifndef PFNGLEGLIMAGETARGETTEXTURE2DOESPROC_DEFINED
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum, GLeglImageOES);
#define PFNGLEGLIMAGETARGETTEXTURE2DOESPROC_DEFINED
#endif

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC g_glEGLImageTargetTexture2DOES = nullptr;

static void* resolveGLProc(const char* name) {
#ifdef GLX_VERSION_1_3
  if (glXGetCurrentContext() != nullptr) {
    // Estamos em um contexto GLX → resolva pelo driver GL
    return (void*)glXGetProcAddressARB((const GLubyte*)name);
  }
#endif
  // Caso contrário, tente EGL (se o contexto/loader for EGL)
  return (void*)eglGetProcAddress(name);
}

static bool ensureGlEglImageProcResolved() {
  if (g_glEGLImageTargetTexture2DOES) return true;
  g_glEGLImageTargetTexture2DOES =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)resolveGLProc("glEGLImageTargetTexture2DOES");
  return g_glEGLImageTargetTexture2DOES != nullptr;
}

std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
               .ToString();
}

// Global flag to check if CEF was initialized
extern bool g_cefInitialized;

// Static member initialization
std::vector<UICEFWebView*> UICEFWebView::s_activeWebViews;
std::mutex UICEFWebView::s_activeWebViewsMutex;

#if defined(__linux__)
bool UICEFWebView::s_glxContextInitialized = false;
void* UICEFWebView::s_x11Display = nullptr;
void* UICEFWebView::s_glxContext = nullptr;
void* UICEFWebView::s_glxPbuffer = nullptr;
void* UICEFWebView::s_glxMainContext = nullptr;
void* UICEFWebView::s_glxDrawable = nullptr;
bool UICEFWebView::s_eglSidecarInitialized = false;
void* UICEFWebView::s_eglDisplay = nullptr;
void* UICEFWebView::s_eglContext = nullptr;
#endif



UICEFWebView::UICEFWebView()
    : UIWebView(), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_cefImage(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0), m_lastMousePos(0, 0), m_isValid(true)
#if defined(__linux__)
    , m_currentTextureIndex(0), m_acceleratedTexturesCreated(false), m_readFbo(0), m_lastReadyTexture(0), m_lastFence(nullptr), m_writeIndex(0)
#endif
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    
    // Thread-safe addition to active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.push_back(this);
    }
    
#if defined(__linux__)
    // Initialize GPU acceleration resources
    m_acceleratedTextures[0] = 0;
    m_acceleratedTextures[1] = 0;
    m_textureFences[0] = nullptr;
    m_textureFences[1] = nullptr;
    
    // Initialize EGL sidecar for DMA buffer import
    initializeEGLSidecar();
    // Initialize shared GLX context
    initializeGLXSharedContext();
#endif
}

UICEFWebView::UICEFWebView(UIWidgetPtr parent)
    : UIWebView(parent), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_cefImage(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0), m_lastMousePos(0, 0), m_isValid(true)
#if defined(__linux__)
    , m_currentTextureIndex(0), m_acceleratedTexturesCreated(false), m_readFbo(0), m_lastReadyTexture(0), m_lastFence(nullptr), m_writeIndex(0)
#endif
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    
    // Thread-safe addition to active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.push_back(this);
    }
    
#if defined(__linux__)
    // Initialize GPU acceleration resources
    m_acceleratedTextures[0] = 0;
    m_acceleratedTextures[1] = 0;
    m_textureFences[0] = nullptr;
    m_textureFences[1] = nullptr;
    
    // Initialize EGL sidecar for DMA buffer import
    initializeEGLSidecar();
    // Initialize shared GLX context
    initializeGLXSharedContext();
#endif
}

UICEFWebView::~UICEFWebView()
{
    g_logger.info("UICEFWebView: Destructor called");
    
    // Mark as invalid FIRST to prevent scheduled events from using this object
    m_isValid.store(false);
    
    // Thread-safe removal from active list
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        auto it = std::find(s_activeWebViews.begin(), s_activeWebViews.end(), this);
        if (it != s_activeWebViews.end()) {
            s_activeWebViews.erase(it);
            g_logger.info(stdext::format("UICEFWebView: Removed from active list. Remaining: %d", s_activeWebViews.size()));
        }
    }
    
    if (m_browser) {
        g_logger.info("UICEFWebView: Closing browser");
        m_browser->GetHost()->CloseBrowser(true);
        m_browser = nullptr;
    }
    m_client = nullptr;
    g_logger.info("UICEFWebView: Destructor completed");
}

void UICEFWebView::createWebView()
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot create browser - CEF not initialized");
        return;
    }

    g_logger.info("UICEFWebView: Creating browser...");

    // Create client
    m_client = new SimpleCEFClient(this);
    g_logger.info("UICEFWebView: Client created successfully");

    // Browser settings
    CefBrowserSettings browser_settings;
    g_logger.info("UICEFWebView: Browser settings configured");

    int maxFps = g_app.getForegroundPaneMaxFps();
    if (maxFps <= 0)
        maxFps = 60;
    browser_settings.windowless_frame_rate = maxFps;

    // Window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(0); // 0 = no parent window
   
    window_info.shared_texture_enabled = true;
    // window_info.external_begin_frame_enabled = true; // Not needed with multi_threaded_message_loop = true

    g_logger.info("UICEFWebView: Window info configured for off-screen rendering");

    // Create browser asynchronously
    bool result = CefBrowserHost::CreateBrowser(window_info, m_client, "about:blank", browser_settings, nullptr, nullptr);
    g_logger.info(stdext::format("UICEFWebView: CreateBrowser called, result: %d", result));
    
    if (!result) {
        g_logger.error("Failed to create CEF browser");
        return;
    }

    g_logger.info("UICEFWebView: Browser creation initiated...");
    // Browser will be available in the first OnPaint callback
}

void UICEFWebView::loadUrlInternal(const std::string& url)
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot load URL - CEF not initialized");
        return;
    }

    if (!m_browser) {
        createWebView();
        // Store URL to load when browser is ready
        m_pendingUrl = url;
        g_logger.info("UICEFWebView: Browser creation in progress, URL will be loaded when ready");
        return;
    }

    g_logger.info("UICEFWebView: Loading URL: " + url);
    
    CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
    if (frame) {
        frame->LoadURL(url);
    }
}

bool UICEFWebView::loadHtmlInternal(const std::string& html, const std::string& baseUrl)
{
    if (!g_cefInitialized) {
        g_logger.error("UICEFWebView: Cannot load HTML - CEF not initialized");
        return false;
    }

    if (!m_browser) {
        createWebView();
        g_logger.info("UICEFWebView: Browser creation in progress, HTML will be loaded when ready");
        m_pendingHtml = html; // Store for later loading
        return false;
    }

    g_logger.info("UICEFWebView: Loading HTML directly into browser");
    // Use a simple data URL with minimal encoding
        std::string dataUri = GetDataURI(html, "text/html");
        g_logger.info(stdext::format("UICEFWebView: Data URL length: %d", html.length()));
        g_logger.info("UICEFWebView: HTML content: " + html.substr(0, 100) + "...");
    m_browser->GetMainFrame()->LoadURL(dataUri);

    return true;
}

void UICEFWebView::executeJavaScriptInternal(const std::string& script)
{
    if (!g_cefInitialized || !m_browser)
        return;
    CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
    if (frame)
        frame->ExecuteJavaScript(script, frame->GetURL(), 0);
}

void UICEFWebView::onCEFPaint(const void* buffer, int width, int height,
                              const CefRenderHandler::RectList& dirtyRects)
{
    if (!buffer || width <= 0 || height <= 0)
        return;

    const bool useBGRA = g_graphics.canUseBGRA();

    // Cria ou redimensiona a imagem em cache
    if (!m_cefImage || m_cefImage->getSize() != Size(width, height)) {
        m_cefImage = ImagePtr(new Image(Size(width, height), 4));
        m_textureCreated = false;
    }

    const uint8_t* bgra = static_cast<const uint8_t*>(buffer);
    uint8_t* dest = m_cefImage->getPixelData();

    // Função utilitária para copiar um retângulo do buffer CEF para a imagem dest
    auto copyRect = [&](int x, int y, int w, int h) {
        for (int row = 0; row < h; ++row) {
            const uint8_t* srcRow = bgra + ((y + row) * width + x) * 4;
            uint8_t* dstRow = dest + ((y + row) * width + x) * 4;

            if (useBGRA) {
                memcpy(dstRow, srcRow, w * 4);
            } else {
                // Conversão BGRA→RGBA caso BGRA não seja suportado pela API gráfica
                for (int col = 0; col < w; ++col) {
                    dstRow[col*4 + 0] = srcRow[col*4 + 2];
                    dstRow[col*4 + 1] = srcRow[col*4 + 1];
                    dstRow[col*4 + 2] = srcRow[col*4 + 0];
                    dstRow[col*4 + 3] = srcRow[col*4 + 3];
                }
            }
        }
    };

    // Copia as áreas modificadas do buffer CEF para a imagem local
    if (dirtyRects.empty()) {
        copyRect(0, 0, width, height);
    } else {
        for (const auto& r : dirtyRects) {
            copyRect(r.x, r.y, r.width, r.height);
        }
    }

    // (re)Cria a textura se necessário
    if (!m_textureCreated || width != m_lastWidth || height != m_lastHeight) {
        m_cefTexture = TexturePtr(new Texture(m_cefImage, false, false, useBGRA));
        m_textureCreated = true;
        m_lastWidth = width;
        m_lastHeight = height;
    } else {
        if (dirtyRects.empty()) {
            // imagem completa: pode usar dest diretamente porque stride == width
            Rect full(0, 0, width, height);
            m_cefTexture->updateSubPixels(full, dest, 4, useBGRA);
        } else {
            // retângulos sujos: precisamos criar um buffer temporário contíguo
            for (const auto& r : dirtyRects) {
                Rect upRect(r.x, r.y, r.width, r.height);

                // buffer contíguo para o sub‑retângulo
                std::vector<uint8_t> temp(r.width * r.height * 4);
                const size_t bytesPerRow = r.width * 4;

                for (int row = 0; row < r.height; ++row) {
                    const uint8_t* srcRow = dest + ((r.y + row) * width + r.x) * 4;
                    uint8_t* dstRow = temp.data() + row * bytesPerRow;
                    memcpy(dstRow, srcRow, bytesPerRow);
                }

                // envia a região com stride correto
                m_cefTexture->updateSubPixels(upRect, temp.data(), 4, useBGRA);
            }
        }
    }

    setVisible(true);
}

void UICEFWebView::onCEFAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
#if defined(_WIN32) && defined(OPENGL_ES) && OPENGL_ES == 2
    HANDLE sharedHandle = static_cast<HANDLE>(info.shared_texture_handle);
    if (!sharedHandle) {
        g_logger.error("UICEFWebView: No shared texture handle available");
        return;
    }

    // Abrir o handle como textura D3D11
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, &fl, &d3dContext))) {
        g_logger.error("UICEFWebView: Failed to create D3D11 device");
        return;
    }

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(d3dDevice->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (void**)&tex))) {
        g_logger.error("UICEFWebView: OpenSharedResource failed");
        d3dDevice->Release();
        d3dContext->Release();
        return;
    }

    // Agora sim passar para ANGLE via eglCreatePbufferFromClientBuffer
    EGLDisplay display = eglGetCurrentDisplay();
    EGLConfig config;
    EGLint numCfg;
    EGLint cfgAttrs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
        EGL_NONE
    };
    eglChooseConfig(display, cfgAttrs, &config, 1, &numCfg);

    EGLint attrs[] = {
        EGL_WIDTH, getWidth(),
        EGL_HEIGHT, getHeight(),
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE
    };

    EGLSurface pbuffer = eglCreatePbufferFromClientBuffer(
        display,
        EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
        (EGLClientBuffer)sharedHandle,  // precisa ser EGLClientBuffer
        config,
        attrs);

    if (pbuffer == EGL_NO_SURFACE) {
        EGLint eglError = eglGetError();
        g_logger.error(stdext::format("eglCreatePbufferFromClientBuffer failed - EGL error: 0x%x (%s)", 
                                    eglError, getEGLErrorString(eglError)));
        tex->Release();
        d3dDevice->Release();
        d3dContext->Release();
        return;
    } else {
        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);
        g_logger.info("UICEFWebView: Accelerated frame bound!");
        eglReleaseTexImage(display, pbuffer, EGL_BACK_BUFFER);
        eglDestroySurface(display, pbuffer);
    }

    tex->Release();
    d3dDevice->Release();
    d3dContext->Release();
#elif defined(__linux__)
    // Validate basic requirements
    if (info.plane_count <= 0 || info.planes[0].fd == -1) {
        g_logger.error("UICEFWebView: Invalid DMA buffer info");
        return;
    }

    // Quick validation
    int width = info.extra.coded_size.width;
    int height = info.extra.coded_size.height;
    int stride = info.planes[0].stride;
    
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192 || stride <= 0) {
        g_logger.error(stdext::format("UICEFWebView: Invalid DMA buffer dimensions: %dx%d, stride: %d", width, height, stride));
        return;
    }
    
    // CRITICAL: Must process immediately! CEF releases the resource after this callback returns
    if (this) {
        this->processAcceleratedPaintGPU(info);
    }
#else
    (void)info;
    g_logger.error("UICEFWebView: GPU acceleration not implemented for this platform!");
#endif
}

void UICEFWebView::initializeGLXSharedContext()
{
#if defined(__linux__)
    if (s_glxContextInitialized) {
        return;
    }
    
    g_logger.info("UICEFWebView: Initializing GLX shared context for CEF thread...");
    
    // Get X11 display from current GLX context
    Display* x11Display = glXGetCurrentDisplay();
    if (!x11Display) {
        g_logger.error("UICEFWebView: No current GLX display - OTClient may not be using GLX");
        return;
    }
    
    s_x11Display = x11Display;
    g_logger.info("UICEFWebView: Found X11 display from GLX");

    // Get main GLX context and drawable
    GLXContext mainContext = glXGetCurrentContext();
    GLXDrawable mainDrawable = glXGetCurrentDrawable();
    if (!mainContext || !mainDrawable) {
        g_logger.error("UICEFWebView: No main GLX context or drawable available");
        return;
    }
    g_logger.info("UICEFWebView: Found main GLX context and drawable");
    s_glxMainContext = mainContext;
    s_glxDrawable = (void*)mainDrawable;
    
    // Choose a GLX FB config
    int fbConfigAttribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        X11_None
    };
    
    int numConfigs;
    GLXFBConfig* fbConfigs = glXChooseFBConfig(x11Display, DefaultScreen(x11Display), fbConfigAttribs, &numConfigs);
    if (!fbConfigs || numConfigs == 0) {
        g_logger.error("UICEFWebView: Failed to choose GLX FB config");
        return;
    }
    
    GLXFBConfig fbConfig = fbConfigs[0];
    XFree(fbConfigs);
    g_logger.info("UICEFWebView: Chosen GLX FB config");
    
    // Create 1x1 Pbuffer for the shared context
    int pbufferAttribs[] = {
        GLX_PBUFFER_WIDTH, 1,
        GLX_PBUFFER_HEIGHT, 1,
        X11_None
    };
    
    GLXPbuffer pbuffer = glXCreatePbuffer(x11Display, fbConfig, pbufferAttribs);
    if (!pbuffer) {
        g_logger.error("UICEFWebView: Failed to create GLX Pbuffer");
        return;
    }
    
    s_glxPbuffer = (void*)pbuffer;
    g_logger.info("UICEFWebView: Created GLX Pbuffer");
    
    // Create shared GLX context
    GLXContext sharedContext = glXCreateNewContext(x11Display, fbConfig, GLX_RGBA_TYPE, mainContext, True);
    if (!sharedContext) {
        g_logger.error("UICEFWebView: Failed to create shared GLX context");
        return;
    }
    
    // Log detalhado da criação do contexto compartilhado
    std::thread::id threadId = std::this_thread::get_id();
    g_logger.info(stdext::format("GLX Shared Context created on thread ID: %s", 
                                std::to_string(std::hash<std::thread::id>{}(threadId)).c_str()));
    g_logger.info(stdext::format("GLX Shared Context: main=%p, shared=%p, display=%p", 
                                mainContext, sharedContext, x11Display));
    
    s_glxContext = sharedContext;
    s_glxContextInitialized = true;
    
    g_logger.info("UICEFWebView: GLX shared context created successfully");
#endif
}

void UICEFWebView::initializeEGLSidecar()
{
#if defined(__linux__)
    // Log da thread atual na inicialização do EGL sidecar
    std::thread::id threadId = std::this_thread::get_id();
    g_logger.info(stdext::format("EGL Sidecar initialization running on thread ID: %s", 
                                std::to_string(std::hash<std::thread::id>{}(threadId)).c_str()));
    
    if (s_eglSidecarInitialized) {
        return;
    }
    
    // Create EGL display for sidecar
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        return;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        return;
    }
    
    // Check for required extensions
    const char* extensions = eglQueryString(eglDisplay, EGL_EXTENSIONS);
    if (!extensions || 
        !strstr(extensions, "EGL_EXT_image_dma_buf_import") ||
        !strstr(extensions, "EGL_KHR_image_base") ||
        !strstr(extensions, "EGL_KHR_gl_texture_2D_image")) {
        return;
    }
    
    g_logger.info("UICEFWebView: Required EGL extensions found");
    
    // NÃO criar contexto EGL separado - usar apenas o display para EGLImage
    // O contexto GLX existente deve ser suficiente para glEGLImageTargetTexture2DOES
    s_eglDisplay = eglDisplay;
    s_eglContext = nullptr; // Não criar contexto EGL separado
    s_eglSidecarInitialized = true;
    
    // Log success only once
    static bool logged = false;
    if (!logged) {
        g_logger.info("UICEFWebView: EGL sidecar initialized successfully (display only, no separate context)");
        logged = true;
    }


#endif
}

void UICEFWebView::cleanupGPUResources()
{
#if defined(__linux__)
    if (s_glxContextInitialized) {
        if (s_glxContext) {
            glXDestroyContext((Display*)s_x11Display, (GLXContext)s_glxContext);
        }
        if (s_glxPbuffer) {
            glXDestroyPbuffer((Display*)s_x11Display, (GLXPbuffer)s_glxPbuffer);
        }
        s_glxContext = nullptr;
        s_glxPbuffer = nullptr;
        s_glxMainContext = nullptr;
        s_glxDrawable = nullptr;
        s_glxContextInitialized = false;
    }
    
    if (s_eglSidecarInitialized) {
        // Não destruir contexto EGL pois não criamos um separado
        eglTerminate((EGLDisplay)s_eglDisplay);
        s_eglDisplay = nullptr;
        s_eglContext = nullptr;
        s_eglSidecarInitialized = false;
    }
    
    if (s_x11Display) {
        XCloseDisplay((Display*)s_x11Display);
        s_x11Display = nullptr;
    }
    
    g_logger.info("UICEFWebView: GPU resources cleaned up");
#endif
}

static bool isDmaBufModifierSupported(EGLDisplay display, EGLint format, uint64_t modifier)
{
#if defined(__linux__)
    auto eglQueryDmaBufFormatsEXTFn = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    auto eglQueryDmaBufModifiersEXTFn = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    if (!eglQueryDmaBufFormatsEXTFn || !eglQueryDmaBufModifiersEXTFn) {
        return false;
    }

    EGLint numFormats = 0;
    if (!eglQueryDmaBufFormatsEXTFn(display, 0, nullptr, &numFormats) || numFormats <= 0) {
        return false;
    }
    std::vector<EGLint> formats(numFormats);
    if (!eglQueryDmaBufFormatsEXTFn(display, numFormats, formats.data(), &numFormats)) {
        return false;
    }
    if (std::find(formats.begin(), formats.end(), format) == formats.end()) {
        return false;
    }

    EGLint numModifiers = 0;
    if (!eglQueryDmaBufModifiersEXTFn(display, format, 0, nullptr, nullptr, &numModifiers) || numModifiers <= 0) {
        return false;
    }
    std::vector<EGLuint64KHR> modifiers(numModifiers);
    std::vector<EGLBoolean> externalOnly(numModifiers);
    if (!eglQueryDmaBufModifiersEXTFn(display, format, numModifiers, modifiers.data(), externalOnly.data(), &numModifiers)) {
        return false;
    }

    return std::find(modifiers.begin(), modifiers.end(), modifier) != modifiers.end();
#else
    (void)display; (void)format; (void)modifier; return false;
#endif
}

static const char* getEGLErrorString(EGLint error)
{
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED - EGL is not initialized, or could not be initialized, for the specified EGL display connection";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS - EGL cannot access a requested resource (for example a context is bound in another thread)";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC - EGL failed to allocate resources for the requested operation";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE - An unrecognized attribute or attribute value was passed in the attribute list";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT - An EGLContext argument does not name a valid EGL rendering context";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG - An EGLConfig argument does not name a valid EGL frame buffer configuration";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE - The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY - An EGLDisplay argument does not name a valid EGL display connection";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE - An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH - Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface)";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER - One or more argument values are invalid";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP - A NativePixmapType argument does not refer to a valid native pixmap";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW - A NativeWindowType argument does not refer to a valid native window";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST - A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering";
        default:
            return "Unknown EGL error";
    }
}

void UICEFWebView::processAcceleratedPaintGPU(const CefAcceleratedPaintInfo& info)
{
#if defined(__linux__)
    if (!s_eglSidecarInitialized) {
        g_logger.error("UICEFWebView: EGL sidecar not initialized");
        return;
    }

    const int fd = info.planes[0].fd;
    const int width = info.extra.coded_size.width;
    const int height = info.extra.coded_size.height;
    const int stride = info.planes[0].stride;
    const int offset = info.planes[0].offset;
    const uint64_t modifier = info.modifier;

    if (fd < 0) {
        g_logger.error(stdext::format("UICEFWebView: Invalid FD received: %d", fd));
        return;
    }

    // dup só para checagem leve (não toque no info nem feche o fd original)
    if (fcntl(fd, F_GETFL) == -1) {
        g_logger.error(stdext::format("UICEFWebView: FD %d is invalid (F_GETFL failed): %s", fd, strerror(errno)));
        return;
    }

    // Duplique FDs separadamente (cada caminho consome o seu)
    int memFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    int eglFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (memFd < 0 && eglFd < 0) { g_logger.error("dup memFd/eglFd failed"); return; }

    // Dispare no thread certo
    g_dispatcher.addEventFromOtherThread([this, memFd, eglFd, width, height, stride, offset, modifier]() mutable {

        auto close_if_owned = [](int& x){ if (x >= 0) { ::close(x); x = -1; } };

        if (!m_isValid.load()) {
            // Object is being destroyed, just clean up and exit
            close_if_owned((int&)memFd);
            close_if_owned((int&)eglFd);
            return;
        }
        // Ensure the main GLX context is current on this thread
        if (!s_glxMainContext || !s_glxDrawable || !s_x11Display) {
            g_logger.error("UICEFWebView: No GLX context or drawable available");
            close_if_owned((int&)memFd);
            close_if_owned((int&)eglFd); 
            return;
        }
        
        // Validate current OpenGL context before proceeding
        GLXContext currentContext = glXGetCurrentContext();
        if (currentContext != s_glxMainContext) {
            if (!glXMakeCurrent((Display*)s_x11Display, (GLXDrawable)s_glxDrawable, (GLXContext)s_glxMainContext)) {
                g_logger.error("UICEFWebView: Failed to make GLX context current");
                close_if_owned((int&)memFd);
                close_if_owned((int&)eglFd);
                return;
            }
        }

        // Verify OpenGL context is valid and ready
        GLenum contextError = glGetError(); // Clear any existing errors
        if (contextError != GL_NO_ERROR) {
            g_logger.warning(stdext::format("UICEFWebView: OpenGL context had pending error: 0x%x", contextError));
        }
        
        // Additional context validation
        GLint maxTextureSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        GLenum getError = glGetError();
        if (getError != GL_NO_ERROR) {
            g_logger.error(stdext::format("UICEFWebView: OpenGL context is invalid - glGetIntegerv failed with error: 0x%x", getError));
            close_if_owned((int&)memFd);
            close_if_owned((int&)eglFd);
            return;
        }
    
        auto eglCreateImageKHRFn = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        auto eglDestroyImageKHRFn = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (!eglCreateImageKHRFn || !eglDestroyImageKHRFn || !ensureGlEglImageProcResolved()) {
            g_logger.error("UICEFWebView: Failed to get EGL functions");
            close_if_owned((int&)memFd);
            close_if_owned((int&)eglFd); 
            return;
        }

        // Heurística: memory_object só quando tem chance de zero-copy
        const bool modifierInvalid = (modifier == 0 || modifier == DRM_FORMAT_MOD_INVALID);
        const bool tightlyPacked   = (stride == width * 4); // ARGB8888 (BGRA em LE)
        const bool preferMemObj    = modifierInvalid && tightlyPacked;

        // Extensão GL para memory objects?
        bool hasMemoryObjectFd = false;
        {
            const char* exts = (const char*)glGetString(GL_EXTENSIONS);
            hasMemoryObjectFd = (exts && strstr(exts, "GL_EXT_memory_object_fd"));
        }

        // CRITICAL FIX: Always recreate texture for each frame
        // Reusing textures with glTexStorageMem2DEXT/glEGLImageTargetTexture2DOES causes GL_INVALID_OPERATION
        m_cefTexture = TexturePtr(new Texture(Size(getWidth(), getHeight())));
        m_textureCreated = true;
        m_lastWidth = getWidth();
        m_lastHeight = getHeight();

        glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
        
        // Set texture parameters that are safe for EGLImage
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Swap red and blue channels to match BGRA frames
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);

        bool done = false;

        // 1) Caminho memory_object_fd (se preferido e disponível)
        if (!done && hasMemoryObjectFd && memFd >= 0) {
            PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT =
                (PFNGLCREATEMEMORYOBJECTSEXTPROC)glXGetProcAddressARB((const GLubyte*)"glCreateMemoryObjectsEXT");
            PFNGLIMPORTMEMORYFDEXTPROC glImportMemoryFdEXT =
                (PFNGLIMPORTMEMORYFDEXTPROC)glXGetProcAddressARB((const GLubyte*)"glImportMemoryFdEXT");
            PFNGLTEXSTORAGEMEM2DEXTPROC glTexStorageMem2DEXT =
                (PFNGLTEXSTORAGEMEM2DEXTPROC)glXGetProcAddressARB((const GLubyte*)"glTexStorageMem2DEXT");
            PFNGLDELETEMEMORYOBJECTSEXTPROC glDeleteMemoryObjectsEXT =
                (PFNGLDELETEMEMORYOBJECTSEXTPROC)glXGetProcAddressARB((const GLubyte*)"glDeleteMemoryObjectsEXT");

            if (glCreateMemoryObjectsEXT && glImportMemoryFdEXT && glTexStorageMem2DEXT && glDeleteMemoryObjectsEXT) {
                GLuint memoryObject = 0;
                glCreateMemoryObjectsEXT(1, &memoryObject);
                GLenum error1 = glGetError();
                
                if (error1 == GL_NO_ERROR && memoryObject != 0) {
                    // Calcular o tamanho do buffer
                    GLuint64 size = (GLuint64)height * stride;

                    // glImportMemoryFdEXT takes ownership of the FD
                    glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, memFd);
                    GLenum error2 = glGetError();
                    
                    if (error2 == GL_NO_ERROR) {     
                        // Use the memory object to create the texture
                        glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height, memoryObject, offset);
                        GLenum error3 = glGetError();
                        
                        if (error3 == GL_NO_ERROR) {         
                            done = true;
                        } else {
                            g_logger.error(stdext::format("ERROR: glTexStorageMem2DEXT failed with error: 0x%x", error3));
                        }

                        ((int&)memFd) = -1;
                    } else {
                        g_logger.error(stdext::format("ERROR: glImportMemoryFdEXT failed with error: 0x%x", error2));
                    }
                    
                    // Always clean up memory object
                    glDeleteMemoryObjectsEXT(1, &memoryObject);
                } else {
                    g_logger.error(stdext::format("ERROR: glCreateMemoryObjectsEXT failed with error: 0x%x", error1));
                }
            } else {
                g_logger.error("UICEFWebView: GL_EXT_memory_object_fd functions not available");
            }
        }

        // 2) Caminho EGLImage (robusto; aceita stride/offset/modifier)
        if (!done && eglFd >= 0) {
            auto buildAttrs = [&](bool includeModifier) {
                std::vector<EGLint> a = {
                    EGL_WIDTH,  width,
                    EGL_HEIGHT, height,
                    // CEF entrega BGRA (little-endian) → DRM_FORMAT_ARGB8888
                    EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
                    EGL_DMA_BUF_PLANE0_FD_EXT, eglFd,
                    EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
                    EGL_DMA_BUF_PLANE0_PITCH_EXT,  stride
                };
                if (includeModifier) {
                    a.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT);
                    a.push_back((EGLint)(modifier & 0xffffffffu));
                    a.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT);
                    a.push_back((EGLint)(modifier >> 32));
                }
                a.push_back(EGL_NONE);
                return a;
            };

            const bool canUseMod = (modifier != DRM_FORMAT_MOD_INVALID && modifier != 0) &&
                                   isDmaBufModifierSupported((EGLDisplay)s_eglDisplay, DRM_FORMAT_ARGB8888, modifier);

            auto attrs = buildAttrs(canUseMod);
            EGLImageKHR img = eglCreateImageKHRFn((EGLDisplay)s_eglDisplay, EGL_NO_CONTEXT,
                                                  EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());

            if (img == EGL_NO_IMAGE_KHR && canUseMod) {
                // tenta sem modifier
                EGLint err = eglGetError();
                g_logger.info(stdext::format("EGLImage w/ modifier failed (0x%x), retrying w/o modifier", err));
                attrs = buildAttrs(false);
                img = eglCreateImageKHRFn((EGLDisplay)s_eglDisplay, EGL_NO_CONTEXT,
                                          EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
            }

            if (img != EGL_NO_IMAGE_KHR) {
                // Após criar a EGLImage, você já pode fechar o FD (posse fica com EGL)
                close_if_owned(eglFd);

                // Vincula a EGLImage à textura
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
                // alguns drivers gostam de ter storage; opcional:
                // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

                g_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
                GLenum glErr = glGetError();

                eglDestroyImageKHRFn((EGLDisplay)s_eglDisplay, img);

                if (glErr == GL_NO_ERROR) {
                    done = true;
                } else {
                    g_logger.error(stdext::format("glEGLImageTargetTexture2DOES failed: 0x%x", glErr));
                }
            } else {
                EGLint err = eglGetError();
                g_logger.error(stdext::format("eglCreateImageKHR failed: 0x%x", err));
            }
        }

        // Limpeza de FDs que ainda estiverem conosco
        close_if_owned(memFd);
        close_if_owned(eglFd);

        glBindTexture(GL_TEXTURE_2D, 0);

        if (!done) {
            g_logger.error("UICEFWebView: GPU import failed; consider CPU fallback here.");
            // implementCPUFallback(...);  // se quiser
            return;
        }

        // sucesso
    });
#endif
}

void UICEFWebView::createAcceleratedTextures(int width, int height)
{
#if defined(__linux__)
    if (!m_acceleratedTexturesCreated || width != m_lastWidth || height != m_lastHeight) {
        // Clean up old resources
        if (m_acceleratedTexturesCreated) {
            glDeleteTextures(2, m_acceleratedTextures);
            if (m_readFbo) {
                glDeleteFramebuffers(1, &m_readFbo);
                m_readFbo = 0;
            }
            for (int i = 0; i < 2; i++) {
                if (m_textureFences[i]) {
                    glDeleteSync(m_textureFences[i]);
                    m_textureFences[i] = nullptr;
                }
            }
        }
        
        // Create ping-pong textures
        glGenTextures(2, m_acceleratedTextures);
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, m_acceleratedTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        }
        
        // Create read FBO for copying
        glGenFramebuffers(1, &m_readFbo);
        
        m_acceleratedTexturesCreated = true;
        m_lastWidth = width;
        m_lastHeight = height;
        
        g_logger.info("UICEFWebView: Created ping-pong textures " + 
                      std::to_string(m_acceleratedTextures[0]) + " and " + 
                      std::to_string(m_acceleratedTextures[1]) + " with FBO " + std::to_string(m_readFbo));
    }
#endif
}

// CPU fallback removed - GPU acceleration is required


void UICEFWebView::onBrowserCreated(CefRefPtr<CefBrowser> browser)
{
    g_logger.info("UICEFWebView: Browser created successfully!");
    m_browser = browser;
    
    if (!m_pendingHtml.empty()) {
        g_logger.info("UICEFWebView: Loading pending HTML content...");
        g_logger.info(stdext::format("UICEFWebView: HTML content length: %d", m_pendingHtml.length()));
        g_logger.info("UICEFWebView: HTML preview: " + m_pendingHtml.substr(0, 200) + "...");

        if(!loadHtmlInternal(m_pendingHtml, "")) {
            g_logger.error("UICEFWebView: Failed to load pending HTML content");
        }
        
        m_pendingHtml.clear();
    }

    if (!m_pendingUrl.empty()) {
        g_logger.info("UICEFWebView: Loading pending URL: " + m_pendingUrl);
        CefRefPtr<CefFrame> frame = m_browser->GetMainFrame();
        if (frame) {
            frame->LoadURL(m_pendingUrl);
        }
        m_pendingUrl.clear();
    }
    
    // With multi_threaded_message_loop = true, CEF handles rendering automatically
    // No manual frame triggering needed
}

void UICEFWebView::drawSelf(Fw::DrawPane drawPane)
{
    // Render CEF content using multi-threaded rendering
    
    // Note: Accelerated frames are now uploaded directly to m_cefTexture via scheduleEvent
    // So we just use the normal rendering path below
    
    // Fallback to regular CEF texture or UIWidget background
    if (m_textureCreated && m_cefTexture) {
        Rect rect = getRect();
        
        // Render the CEF texture using OTClient's graphics system
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    } else {
        // Fallback to UIWidget background when no CEF texture is available
        UIWidget::drawSelf(drawPane);
    }
}

bool UICEFWebView::onMousePress(const Point& mousePos, Fw::MouseButton button)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMousePress(m_browser, localPos, button);

    return UIWidget::onMousePress(mousePos, button);
}

bool UICEFWebView::onMouseRelease(const Point& mousePos, Fw::MouseButton button)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMouseRelease(m_browser, localPos, button);

    return UIWidget::onMouseRelease(mousePos, button);
}

bool UICEFWebView::onMouseMove(const Point& mousePos, const Point& mouseMoved)
{
    Point localPos = mousePos - getPosition();
    m_lastMousePos = localPos;
    if (m_browser)
        CefInputHandler::handleMouseMove(m_browser, localPos, !containsPoint(mousePos));

    return UIWidget::onMouseMove(mousePos, mouseMoved);
}

bool UICEFWebView::onMouseWheel(const Point& mousePos, Fw::MouseWheelDirection direction)
{
    Point localPos = mousePos - getPosition();
    if (m_browser)
        CefInputHandler::handleMouseWheel(m_browser, localPos, direction);

    return UIWidget::onMouseWheel(mousePos, direction);
}

void UICEFWebView::onHoverChange(bool hovered)
{
    if (!hovered && m_browser)
        CefInputHandler::handleMouseMove(m_browser, m_lastMousePos, true);

    UIWidget::onHoverChange(hovered);
}

bool UICEFWebView::onKeyDown(uchar keyCode, int keyboardModifiers)
{
    if (m_browser)
        CefInputHandler::handleKeyDown(m_browser, keyCode);

    return UIWidget::onKeyDown(keyCode, keyboardModifiers);
}

bool UICEFWebView::onKeyPress(uchar keyCode, int keyboardModifiers, int autoRepeatTicks)
{
    if (m_browser && autoRepeatTicks > 0)
        CefInputHandler::handleKeyPress(m_browser, keyCode);

    return UIWidget::onKeyPress(keyCode, keyboardModifiers, autoRepeatTicks);
}

bool UICEFWebView::onKeyText(const std::string& keyText)
{
    if (m_browser && !keyText.empty())
        CefInputHandler::handleKeyText(m_browser, keyText);

    return UIWidget::onKeyText(keyText);
}

bool UICEFWebView::onKeyUp(uchar keyCode, int keyboardModifiers)
{
    if (m_browser)
        CefInputHandler::handleKeyUp(m_browser, keyCode);

    return UIWidget::onKeyUp(keyCode, keyboardModifiers);
}

// Event handlers with correct signatures
void UICEFWebView::onLoadStarted() {}
void UICEFWebView::onLoadFinished(bool success) {}
void UICEFWebView::onUrlChanged(const std::string& url) {}
void UICEFWebView::onTitleChanged(const std::string& title) {}
void UICEFWebView::onJavaScriptCallback(const std::string& name, const std::string& data) {
    UIWebView::onJavaScriptCallback(name, data);
}

void UICEFWebView::onGeometryChange(const Rect& oldRect, const Rect& newRect)
{
    // Call parent implementation first
    UIWidget::onGeometryChange(oldRect, newRect);
    
    // Check if size actually changed
    if (oldRect.size() != newRect.size()) {
        g_logger.info("UICEFWebView: Size changed from " + 
                     std::to_string(oldRect.width()) + "x" + std::to_string(oldRect.height()) + 
                     " to " + std::to_string(newRect.width()) + "x" + std::to_string(newRect.height()));
        
        // CRITICAL: Clean up OpenGL resources before changing size
        if (m_textureCreated && m_cefTexture) {
            g_logger.info("UICEFWebView: Cleaning up OpenGL texture before resize");
            
            // Ensure we're in the correct OpenGL context
            if (s_glxMainContext && s_glxDrawable && s_x11Display) {
                GLXContext currentContext = glXGetCurrentContext();
                if (currentContext != s_glxMainContext) {
                    if (!glXMakeCurrent((Display*)s_x11Display, (GLXDrawable)s_glxDrawable, (GLXContext)s_glxMainContext)) {
                        g_logger.warning("UICEFWebView: Failed to make GLX context current during cleanup");
                    }
                }
                
                // Clear any pending OpenGL errors before cleanup
                while (glGetError() != GL_NO_ERROR) { /* clear error queue */ }
                
                // Unbind any bound textures
                glBindTexture(GL_TEXTURE_2D, 0);
                
                // Force texture destruction to free GPU memory
                m_cefTexture.reset();
                
                g_logger.info("UICEFWebView: OpenGL resources cleaned up successfully");
            }
        }
        
        // Notify CEF browser about the size change
        if (m_browser) {
            CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
            if (host) {
                host->WasResized();
                g_logger.info("UICEFWebView: Notified CEF browser about size change");
            }
        }
        
        // Reset texture state to force recreation with clean slate
        m_textureCreated = false;
        m_lastWidth = 0;
        m_lastHeight = 0;
        m_cefTexture.reset(); // Ensure texture pointer is also reset
        
        g_logger.info("UICEFWebView: Texture state reset for recreation");
    }
}

// Static methods for managing all WebViews
void UICEFWebView::closeAllWebViews() {
    std::vector<UICEFWebView*> webViewsToClose;
    
    // Thread-safe copy of the vector
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        g_logger.info(stdext::format("UICEFWebView: Closing all active WebViews. Count: %d", s_activeWebViews.size()));
        webViewsToClose = s_activeWebViews;
    }
    
    for (auto* webview : webViewsToClose) {
        if (webview && webview->m_browser) {
            g_logger.info("UICEFWebView: Force closing browser");
            webview->m_browser->GetHost()->CloseBrowser(true);
            webview->m_browser = nullptr;
        }
    }
    
    // Thread-safe clear
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        s_activeWebViews.clear();
    }
    g_logger.info("UICEFWebView: All WebViews closed");
}

size_t UICEFWebView::getActiveWebViewCount() {
    std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
    return s_activeWebViews.size();
}

void UICEFWebView::setWindowlessFrameRate(int fps)
{
    if (fps <= 0)
        fps = 60;

    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host)
            host->SetWindowlessFrameRate(fps);
    }
}

void UICEFWebView::setAllWindowlessFrameRate(int fps)
{
    std::vector<UICEFWebView*> webViewsCopy;
    
    // Thread-safe copy for iteration
    {
        std::lock_guard<std::mutex> lock(s_activeWebViewsMutex);
        webViewsCopy = s_activeWebViews;
    }
    
    for (auto* webview : webViewsCopy) {
        if (webview)
            webview->setWindowlessFrameRate(fps);
    }
}



#endif // USE_CEF 
