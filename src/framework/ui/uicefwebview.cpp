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
#include <framework/platform/platformwindow.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string>
#include <vector>
#include <unordered_map>
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

// Define BGRA format constants if not available
#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT 0x93A1
#endif

// Define DRM BGRA format if not available
#ifndef DRM_FORMAT_BGRA8888
#define DRM_FORMAT_BGRA8888 0x34324742 /* [31:0] B:G:R:A 8:8:8:8 little endian */
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
#include "cefphysfsresourcehandler.h"
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

static int getCefModifiers()
{
    int mods = 0;

    int keyMods = g_window.getKeyboardModifiers();
    if (keyMods & Fw::KeyboardCtrlModifier)
        mods |= EVENTFLAG_CONTROL_DOWN;
    if (keyMods & Fw::KeyboardShiftModifier)
        mods |= EVENTFLAG_SHIFT_DOWN;
    if (keyMods & Fw::KeyboardAltModifier)
        mods |= EVENTFLAG_ALT_DOWN;

    if (g_window.isMouseButtonPressed(Fw::MouseLeftButton))
        mods |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (g_window.isMouseButtonPressed(Fw::MouseRightButton))
        mods |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (g_window.isMouseButtonPressed(Fw::MouseMidButton))
        mods |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;

    return mods;
}

static int translateKeyCode(uchar key)
{
    static const std::unordered_map<uchar, int> vkMap = {
        {Fw::KeyBackspace, 0x08}, // VK_BACK
        {Fw::KeyTab, 0x09},       // VK_TAB
        {Fw::KeyEnter, 0x0D},     // VK_RETURN
        {Fw::KeyEscape, 0x1B},    // VK_ESCAPE
        {Fw::KeyPageUp, 0x21},    // VK_PRIOR
        {Fw::KeyPageDown, 0x22},  // VK_NEXT
        {Fw::KeyEnd, 0x23},       // VK_END
        {Fw::KeyHome, 0x24},      // VK_HOME
        {Fw::KeyLeft, 0x25},      // VK_LEFT
        {Fw::KeyUp, 0x26},        // VK_UP
        {Fw::KeyRight, 0x27},     // VK_RIGHT
        {Fw::KeyDown, 0x28},      // VK_DOWN
        {Fw::KeyInsert, 0x2D},    // VK_INSERT
        {Fw::KeyDelete, 0x2E},    // VK_DELETE
        {Fw::KeyPause, 0x13},     // VK_PAUSE
        {Fw::KeyPrintScreen, 0x2C}, // VK_SNAPSHOT
        {Fw::KeyNumLock, 0x90},   // VK_NUMLOCK
        {Fw::KeyScrollLock, 0x91},// VK_SCROLL
        {Fw::KeyCapsLock, 0x14},  // VK_CAPITAL
        {Fw::KeyCtrl, 0x11},      // VK_CONTROL
        {Fw::KeyShift, 0x10},     // VK_SHIFT
        {Fw::KeyAlt, 0x12},       // VK_MENU
        {Fw::KeyMeta, 0x5B},      // VK_LWIN
        {Fw::KeyMenu, 0x5D},      // VK_APPS
        {Fw::KeySpace, 0x20},     // VK_SPACE
        {Fw::KeyExclamation, 0x31}, // '1' key
        {Fw::KeyQuote, 0xDE},
        {Fw::KeyNumberSign, 0x33},
        {Fw::KeyDollar, 0x34},
        {Fw::KeyPercent, 0x35},
        {Fw::KeyAmpersand, 0x37},
        {Fw::KeyApostrophe, 0xDE},
        {Fw::KeyLeftParen, 0x39},
        {Fw::KeyRightParen, 0x30},
        {Fw::KeyAsterisk, 0x38},
        {Fw::KeyPlus, 0xBB},
        {Fw::KeyComma, 0xBC},
        {Fw::KeyMinus, 0xBD},
        {Fw::KeyPeriod, 0xBE},
        {Fw::KeySlash, 0xBF},
        {Fw::Key0, 0x30},
        {Fw::Key1, 0x31},
        {Fw::Key2, 0x32},
        {Fw::Key3, 0x33},
        {Fw::Key4, 0x34},
        {Fw::Key5, 0x35},
        {Fw::Key6, 0x36},
        {Fw::Key7, 0x37},
        {Fw::Key8, 0x38},
        {Fw::Key9, 0x39},
        {Fw::KeyColon, 0xBA},
        {Fw::KeySemicolon, 0xBA},
        {Fw::KeyLess, 0xBC},
        {Fw::KeyEqual, 0xBB},
        {Fw::KeyGreater, 0xBE},
        {Fw::KeyQuestion, 0xBF},
        {Fw::KeyAtSign, 0x32},
        {Fw::KeyA, 0x41}, {Fw::KeyB, 0x42}, {Fw::KeyC, 0x43}, {Fw::KeyD, 0x44},
        {Fw::KeyE, 0x45}, {Fw::KeyF, 0x46}, {Fw::KeyG, 0x47}, {Fw::KeyH, 0x48},
        {Fw::KeyI, 0x49}, {Fw::KeyJ, 0x4A}, {Fw::KeyK, 0x4B}, {Fw::KeyL, 0x4C},
        {Fw::KeyM, 0x4D}, {Fw::KeyN, 0x4E}, {Fw::KeyO, 0x4F}, {Fw::KeyP, 0x50},
        {Fw::KeyQ, 0x51}, {Fw::KeyR, 0x52}, {Fw::KeyS, 0x53}, {Fw::KeyT, 0x54},
        {Fw::KeyU, 0x55}, {Fw::KeyV, 0x56}, {Fw::KeyW, 0x57}, {Fw::KeyX, 0x58},
        {Fw::KeyY, 0x59}, {Fw::KeyZ, 0x5A},
        {Fw::KeyLeftBracket, 0xDB},
        {Fw::KeyBackslash, 0xDC},
        {Fw::KeyRightBracket, 0xDD},
        {Fw::KeyCaret, 0x36},
        {Fw::KeyUnderscore, 0xBD},
        {Fw::KeyGrave, 0xC0},
        {Fw::KeyLeftCurly, 0xDB},
        {Fw::KeyBar, 0xDC},
        {Fw::KeyRightCurly, 0xDD},
        {Fw::KeyTilde, 0xC0},
        {Fw::KeyF1, 0x70},  {Fw::KeyF2, 0x71},  {Fw::KeyF3, 0x72},
        {Fw::KeyF4, 0x73},  {Fw::KeyF5, 0x74},  {Fw::KeyF6, 0x75},
        {Fw::KeyF7, 0x76},  {Fw::KeyF8, 0x77},  {Fw::KeyF9, 0x78},
        {Fw::KeyF10, 0x79}, {Fw::KeyF11, 0x7A}, {Fw::KeyF12, 0x7B},
        {Fw::KeyNumpad0, 0x60}, {Fw::KeyNumpad1, 0x61}, {Fw::KeyNumpad2, 0x62},
        {Fw::KeyNumpad3, 0x63}, {Fw::KeyNumpad4, 0x64}, {Fw::KeyNumpad5, 0x65},
        {Fw::KeyNumpad6, 0x66}, {Fw::KeyNumpad7, 0x67}, {Fw::KeyNumpad8, 0x68},
        {Fw::KeyNumpad9, 0x69}
    };

    auto it = vkMap.find(key);
    if(it != vkMap.end())
        return it->second;
    return key;
}

static std::u16string cp1252ToUtf16(const std::string& text)
{
    static const char16_t table[32] = {
        0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
        0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
    };

    std::u16string out;
    out.reserve(text.size());
    for(unsigned char ch : text) {
        if(ch < 0x80)
            out.push_back(static_cast<char16_t>(ch));
        else if(ch < 0xA0)
            out.push_back(table[ch - 0x80]);
        else
            out.push_back(0x00A0 + (ch - 0xA0));
    }
    return out;
}

// Simple CEF Client implementation
class SimpleCEFClient : public CefClient,
                        public CefRenderHandler,
                        public CefRequestHandler,
                        public CefLifeSpanHandler {
public:
    explicit SimpleCEFClient(UICEFWebView* webview) : m_webview(webview) {
        CefMessageRouterConfig config;
        config.js_query_function = "luaCallback";
        config.js_cancel_function = "luaCallbackCancel";
        m_messageRouter = CefMessageRouterBrowserSide::Create(config);
        m_routerHandler = std::make_unique<LuaCallbackHandler>(m_webview);
        m_messageRouter->AddHandler(m_routerHandler.get(), false);
    }

    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override {
        std::string url = request->GetURL();
        if (url.rfind("otclient://", 0) == 0 ||
            url.rfind("http://otclient/", 0) == 0 ||
            url.rfind("https://otclient/", 0) == 0) {
            return new CefPhysFsResourceRequestHandler();
        }
        return nullptr;
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override {
        if (m_messageRouter &&
            m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message))
            return true;
        return false;
    }

    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request,
                        bool user_gesture,
                        bool is_redirect) override {
        if (m_messageRouter)
            m_messageRouter->OnBeforeBrowse(browser, frame);
        return false;
    }

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_logger.info("UICEFWebView: OnAfterCreated called - browser is ready!");
        if (m_webview) {
            m_webview->onBrowserCreated(browser);
        }
    }

    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override {
        rect.x = rect.y = 0;
        if (m_webview) {
            Size widgetSize = m_webview->getSize();
            rect.width = widgetSize.width();
            rect.height = widgetSize.height();
        } else {
            rect.width = 600;
            rect.height = 400;
        }
    }

    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width, int height) override {
        static bool sofwareAccelerationLogged = false;
        if (!sofwareAccelerationLogged) {
            g_logger.info("============ UICEFWebView: Software acceleration is enabled =============");
            g_logger.info("UICEFWebView: OnPaint called - using software rendering");
            sofwareAccelerationLogged = true;
        }

        if (m_webview) {
            if (!m_webview->m_browser) {
                m_webview->onBrowserCreated(browser);
            }
            if (type == PET_VIEW) {
                // With multi_threaded_message_loop = true, we're on CEF UI thread
                // Need to schedule paint processing on main thread for OpenGL context
                
                // Copy buffer data since it may be freed after this callback
                std::vector<uint8_t> bufferCopy(width * height * 4);
                memcpy(bufferCopy.data(), buffer, bufferCopy.size());
                
                g_dispatcher.scheduleEvent([webview = m_webview, bufferCopy = std::move(bufferCopy), width, height, dirtyRects]() mutable {
                    if (webview) {
                        webview->onCEFPaint(bufferCopy.data(), width, height, dirtyRects);
                    }
                }, 0);
            }
        }
    }

    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type,
                            const RectList& dirtyRects,
                            const CefAcceleratedPaintInfo& info) override {
        static bool gpuAccelerationLogged = false;
        if (!gpuAccelerationLogged) {
            g_logger.info("============ UICEFWebView: GPU acceleration is enabled =============");
            g_logger.info("UICEFWebView: OnAcceleratedPaint called - using GPU rendering!");
            g_logger.info("UICEFWebView: accelerated paint info received");
            gpuAccelerationLogged = true;
        }
        
        if (m_webview) {
            if (!m_webview->m_browser) {
                g_logger.info("UICEFWebView: Browser created via OnAcceleratedPaint callback");
                m_webview->onBrowserCreated(browser);
            }
            if (type == PET_VIEW) {
                // CRITICAL: Must process immediately! CEF releases the resource after this callback returns
                if (m_webview) {
                    m_webview->processAcceleratedPaintGPU(info);
                }
            }
        }
    }

private:
    class LuaCallbackHandler : public CefMessageRouterBrowserSide::Handler {
    public:
        explicit LuaCallbackHandler(UICEFWebView* webview) : m_webview(webview) {}

        bool OnQuery(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     int64 query_id,
                     const CefString& request,
                     bool persistent,
                     CefRefPtr<Callback> callback) override {
            CefRefPtr<CefValue> value = CefParseJSON(request, JSON_PARSER_RFC);
            if (value && value->GetType() == VTYPE_DICTIONARY) {
                CefRefPtr<CefDictionaryValue> dict = value->GetDictionary();
                std::string name = dict->GetString("name");
                std::string data;
                if (dict->HasKey("data")) {
                    CefRefPtr<CefValue> dataValue = dict->GetValue("data");
                    switch (dataValue->GetType()) {
                        case VTYPE_STRING:
                            // String: usar diretamente
                            data = dataValue->GetString();
                            break;
                        case VTYPE_INT:
                            // Inteiro: converter para string
                            data = std::to_string(dataValue->GetInt());
                            break;
                        case VTYPE_DOUBLE:
                            // Double: converter para string
                            data = std::to_string(dataValue->GetDouble());
                            break;
                        case VTYPE_BOOL:
                            // Boolean: converter para string
                            data = dataValue->GetBool() ? "true" : "false";
                            break;
                        default:
                            // Para tipos complexos (array, object, null), usar JSON
                            data = CefWriteJSON(dataValue, JSON_WRITER_DEFAULT).ToString();
                            break;
                    }
                }
                if (m_webview && !name.empty()) {
                    // With multi_threaded_message_loop = true, we're on CEF UI thread
                    // Need to schedule callback on main thread for thread safety
                    g_dispatcher.scheduleEvent([webview = m_webview, name, data]() {
                        if (webview) {
                            webview->onJavaScriptCallback(name, data);
                        }
                    }, 0);
                    callback->Success("");
                    return true;
                }
            }
            callback->Failure(0, "Invalid request");
            return true;
        }

        void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             int64 query_id) override {}

    private:
        UICEFWebView* m_webview;
    };

    UICEFWebView* m_webview;
    CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;
    std::unique_ptr<LuaCallbackHandler> m_routerHandler;

    IMPLEMENT_REFCOUNTING(SimpleCEFClient);
};

UICEFWebView::UICEFWebView()
    : UIWebView(), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_cefImage(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0), m_lastMousePos(0, 0)
#if defined(__linux__)
    , m_currentTextureIndex(0), m_acceleratedTexturesCreated(false), m_readFbo(0), m_lastReadyTexture(0), m_lastFence(nullptr), m_writeIndex(0)
#endif
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    s_activeWebViews.push_back(this);
    
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
    : UIWebView(parent), m_browser(nullptr), m_client(nullptr), m_cefTexture(nullptr), m_cefImage(nullptr), m_textureCreated(false), m_lastWidth(0), m_lastHeight(0), m_lastMousePos(0, 0)
#if defined(__linux__)
    , m_currentTextureIndex(0), m_acceleratedTexturesCreated(false), m_readFbo(0), m_lastReadyTexture(0), m_lastFence(nullptr), m_writeIndex(0)
#endif
{
    setSize(Size(800, 600)); // Set initial size
    setDraggable(true); // Enable dragging for scrollbar interaction
    s_activeWebViews.push_back(this);
    
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
    
    // Remove from active list
    auto it = std::find(s_activeWebViews.begin(), s_activeWebViews.end(), this);
    if (it != s_activeWebViews.end()) {
        s_activeWebViews.erase(it);
        g_logger.info(stdext::format("UICEFWebView: Removed from active list. Remaining: %d", s_activeWebViews.size()));
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
        g_logger.error("eglCreatePbufferFromClientBuffer failed, eglError=" +
                       stdext::dec_to_hex(eglGetError()));
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

    // Verificar se o FD é válido usando fcntl
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        g_logger.error(stdext::format("UICEFWebView: FD %d is invalid (fcntl F_GETFL failed): %s", fd, strerror(errno)));
        return;
    }

    int dupFd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
    if (dupFd < 0) {
        g_logger.error(stdext::format("UICEFWebView: Failed to duplicate FD %d: %s (errno=%d)", fd, strerror(errno), errno));
        return;
    }    

    // SAFETY: Don't capture 'this' directly to avoid dangling pointer crashes
    // Process GPU acceleration immediately in current thread instead of scheduling
    {
        // Ensure the main GLX context is current on this thread
        if (!s_glxMainContext || !s_glxDrawable || !s_x11Display) {
            g_logger.error("UICEFWebView: No GLX context or drawable available");
            close(dupFd);
            return;
        }
        
        // Validate current OpenGL context before proceeding
        GLXContext currentContext = glXGetCurrentContext();
        if (currentContext != s_glxMainContext) {
            if (!glXMakeCurrent((Display*)s_x11Display, (GLXDrawable)s_glxDrawable, (GLXContext)s_glxMainContext)) {
                g_logger.error("UICEFWebView: Failed to make GLX context current");
                close(dupFd);
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
            close(dupFd);
            return;
        }
    
        auto eglCreateImageKHRFn = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        auto eglDestroyImageKHRFn = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (!eglCreateImageKHRFn || !eglDestroyImageKHRFn || !ensureGlEglImageProcResolved()) {
            g_logger.error("UICEFWebView: Failed to get EGL functions");
            close(dupFd);
            return;
        }

        auto buildAttrs = [&](bool includeModifier) {
            std::vector<EGLint> attrs = {
                EGL_WIDTH, width,
                EGL_HEIGHT, height,
                EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_BGRA8888,
                EGL_DMA_BUF_PLANE0_FD_EXT, dupFd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, stride
            };
            if (includeModifier) {
                attrs.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT);
                attrs.push_back(static_cast<EGLint>(modifier & 0xffffffff));
                attrs.push_back(EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT);
                attrs.push_back(static_cast<EGLint>(modifier >> 32));
            }
            attrs.push_back(EGL_NONE);
            return attrs;
        };

        bool useModifier = modifier != DRM_FORMAT_MOD_INVALID &&
                           isDmaBufModifierSupported((EGLDisplay)s_eglDisplay, DRM_FORMAT_BGRA8888, modifier);

        auto imgAttrs = buildAttrs(useModifier);

        EGLImageKHR img = eglCreateImageKHRFn((EGLDisplay)s_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, imgAttrs.data());

        if (img == EGL_NO_IMAGE_KHR && useModifier) {
            g_logger.info("UICEFWebView: EGL image creation failed with modifier, trying without modifier");
            imgAttrs = buildAttrs(false);
            img = eglCreateImageKHRFn((EGLDisplay)s_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, imgAttrs.data());
        }

        if (img == EGL_NO_IMAGE_KHR) {
            g_logger.error("UICEFWebView: Failed to create EGL image");
            close(dupFd);
            return;
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
                
        // Verificar se temos a extensão GL_EXT_memory_object_fd como alternativa
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        bool hasMemoryObjectFd = (extensions && strstr(extensions, "GL_EXT_memory_object_fd"));
        
        // Try GL_EXT_memory_object_fd approach ONLY ONCE per call
        bool memoryObjectSuccess = false;
        if (hasMemoryObjectFd) {            
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
                    glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, dupFd);
                    GLenum error2 = glGetError();
                    
                    if (error2 == GL_NO_ERROR) {
                        // Use the memory object to create the texture
                        // CRITICAL: Use GL_BGRA8 to match CEF's output format
                        glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_BGRA8_EXT, width, height, memoryObject, offset);
                        GLenum error3 = glGetError();
                        
                        if (error3 == GL_NO_ERROR) {
                            memoryObjectSuccess = true;
                        } else {
                            g_logger.error(stdext::format("ERROR: glTexStorageMem2DEXT failed with error: 0x%x", error3));
                        }
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
        
        // Only try EGLImage approach if memory object failed
        if (!memoryObjectSuccess) {
            g_logger.info("Trying EGLImage approach with existing GLX context...");
            
            if (g_glEGLImageTargetTexture2DOES == nullptr) {
                g_logger.error("g_glEGLImageTargetTexture2DOES is NULL");
                eglDestroyImageKHRFn((EGLDisplay)s_eglDisplay, img);
                if (!hasMemoryObjectFd) close(dupFd);
                return;
            }
            
            // Ensure texture is bound correctly
            glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
            
            // POTENTIAL FIX: Ensure clean OpenGL state before EGLImage operation
            // Some drivers require specific state for glEGLImageTargetTexture2DOES
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            
            // Ensure we're using texture unit 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_cefTexture->getId());
            
            // Clear any pending operations
            glFinish();
            
            // Detailed diagnostics before glEGLImageTargetTexture2DOES
            GLint boundTexture;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
            GLenum preError = glGetError();
            if (preError != GL_NO_ERROR) {
                g_logger.error(stdext::format("OpenGL error before EGLImage operation: 0x%x", preError));
            }
            
            g_logger.info(stdext::format("About to call glEGLImageTargetTexture2DOES - bound texture: %d, target texture: %d", 
                                       boundTexture, m_cefTexture->getId()));
            
            // Check if we have a valid OpenGL context
            GLXContext currentCtx = glXGetCurrentContext();
            if (currentCtx == nullptr) {
                g_logger.error("No current OpenGL context when calling glEGLImageTargetTexture2DOES!");
            }
            
            g_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
            
            GLenum glError = glGetError();
            if (glError != GL_NO_ERROR) {
                g_logger.error(stdext::format("ERROR: GPU acceleration failed with OpenGL error: 0x%x (GL_INVALID_OPERATION=0x502)", glError));
                
                // Additional diagnostics to understand WHY it failed
                GLint textureBinding;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
                
                GLint activeTexture;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
                
                GLboolean isTexture = glIsTexture(m_cefTexture->getId());
                
                g_logger.error(stdext::format("Failure diagnostics - Bound texture: %d, Active texture unit: 0x%x, Is valid texture: %s", 
                             textureBinding, activeTexture, isTexture ? "YES" : "NO"));
            } else {
                g_logger.info("UICEFWebView: GPU acceleration successful via EGLImage!");
            }
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        eglDestroyImageKHRFn((EGLDisplay)s_eglDisplay, img);
        
        // Close FD only if memory object approach didn't take ownership
        if (!memoryObjectSuccess && !hasMemoryObjectFd) {
            close(dupFd);
        }
    }
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefMouseEvent event;
            Point localPos = mousePos - getPosition();
            event.x = localPos.x;
            event.y = localPos.y;
            event.modifiers = getCefModifiers();

            CefBrowserHost::MouseButtonType btnType = MBT_LEFT;
            if (button == Fw::MouseRightButton)
                btnType = MBT_RIGHT;
            else if (button == Fw::MouseMidButton)
                btnType = MBT_MIDDLE;

            host->SendMouseClickEvent(event, btnType, false, 1);
        }
    }

    return UIWidget::onMousePress(mousePos, button);
}

bool UICEFWebView::onMouseRelease(const Point& mousePos, Fw::MouseButton button)
{
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefMouseEvent event;
            Point localPos = mousePos - getPosition();
            event.x = localPos.x;
            event.y = localPos.y;
            event.modifiers = getCefModifiers();

            CefBrowserHost::MouseButtonType btnType = MBT_LEFT;
            if (button == Fw::MouseRightButton)
                btnType = MBT_RIGHT;
            else if (button == Fw::MouseMidButton)
                btnType = MBT_MIDDLE;

            host->SendMouseClickEvent(event, btnType, true, 1);
        }
    }

    return UIWidget::onMouseRelease(mousePos, button);
}

bool UICEFWebView::onMouseMove(const Point& mousePos, const Point& mouseMoved)
{
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefMouseEvent event;
            Point localPos = mousePos - getPosition();
            event.x = localPos.x;
            event.y = localPos.y;
            m_lastMousePos = localPos;
            event.modifiers = getCefModifiers();
            bool leave = !containsPoint(mousePos);
            host->SendMouseMoveEvent(event, leave);
        }
    }

    return UIWidget::onMouseMove(mousePos, mouseMoved);
}

bool UICEFWebView::onMouseWheel(const Point& mousePos, Fw::MouseWheelDirection direction)
{
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefMouseEvent event;
            Point localPos = mousePos - getPosition();
            event.x = localPos.x;
            event.y = localPos.y;
            event.modifiers = getCefModifiers();

            int deltaY = direction == Fw::MouseWheelUp ? 120 : -120;
            host->SendMouseWheelEvent(event, 0, deltaY);
        }
    }

    return UIWidget::onMouseWheel(mousePos, direction);
}

void UICEFWebView::onHoverChange(bool hovered)
{
    if (!hovered && m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefMouseEvent event;
            event.x = m_lastMousePos.x;
            event.y = m_lastMousePos.y;
            event.modifiers = getCefModifiers();
            host->SendMouseMoveEvent(event, true);
        }
    }

    UIWidget::onHoverChange(hovered);
}

bool UICEFWebView::onKeyDown(uchar keyCode, int keyboardModifiers)
{
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefKeyEvent event = {};  // Initialize all fields to zero
            event.type = KEYEVENT_RAWKEYDOWN;
            event.modifiers = getCefModifiers();
            event.windows_key_code = translateKeyCode(keyCode);
            event.native_key_code = event.windows_key_code;
            event.is_system_key = 0;
            event.character = 0;
            event.unmodified_character = 0;
            event.focus_on_editable_field = 0;
            host->SendKeyEvent(event);
        }
    }

    return UIWidget::onKeyDown(keyCode, keyboardModifiers);
}

bool UICEFWebView::onKeyPress(uchar keyCode, int keyboardModifiers, int autoRepeatTicks)
{
    if (m_browser && autoRepeatTicks > 0) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefKeyEvent event = {};  // Initialize all fields to zero
            event.type = KEYEVENT_RAWKEYDOWN;
            event.modifiers = getCefModifiers();
            event.windows_key_code = translateKeyCode(keyCode);
            event.native_key_code = event.windows_key_code;
            event.is_system_key = 0;
            event.character = 0;
            event.unmodified_character = 0;
            event.focus_on_editable_field = 0;
            host->SendKeyEvent(event);
        }
    }

    return UIWidget::onKeyPress(keyCode, keyboardModifiers, autoRepeatTicks);
}

bool UICEFWebView::onKeyText(const std::string& keyText)
{
    if (m_browser && !keyText.empty()) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            std::u16string chars = cp1252ToUtf16(keyText);
            for (char16_t ch : chars) {
                CefKeyEvent event = {};  // Initialize all fields to zero
                event.type = KEYEVENT_CHAR;
                event.modifiers = getCefModifiers();
                event.character = ch;
                event.unmodified_character = ch;
                event.windows_key_code = static_cast<int>(ch);
                event.native_key_code = event.windows_key_code;
                event.is_system_key = 0;
                event.focus_on_editable_field = 0;
                host->SendKeyEvent(event);
            }
        }
    }

    return UIWidget::onKeyText(keyText);
}

bool UICEFWebView::onKeyUp(uchar keyCode, int keyboardModifiers)
{
    if (m_browser) {
        CefRefPtr<CefBrowserHost> host = m_browser->GetHost();
        if (host) {
            CefKeyEvent event = {};  // Initialize all fields to zero
            event.type = KEYEVENT_KEYUP;
            event.modifiers = getCefModifiers();
            event.windows_key_code = translateKeyCode(keyCode);
            event.native_key_code = event.windows_key_code;
            event.is_system_key = 0;
            event.character = 0;
            event.unmodified_character = 0;
            event.focus_on_editable_field = 0;
            host->SendKeyEvent(event);
        }
    }

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
    g_logger.info(stdext::format("UICEFWebView: Closing all active WebViews. Count: %d", s_activeWebViews.size()));
    
    // Create a copy of the vector to avoid issues during iteration
    std::vector<UICEFWebView*> webViewsToClose = s_activeWebViews;
    
    for (auto* webview : webViewsToClose) {
        if (webview && webview->m_browser) {
            g_logger.info("UICEFWebView: Force closing browser");
            webview->m_browser->GetHost()->CloseBrowser(true);
            webview->m_browser = nullptr;
        }
    }
    
    s_activeWebViews.clear();
    g_logger.info("UICEFWebView: All WebViews closed");
}

size_t UICEFWebView::getActiveWebViewCount() {
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
    for (auto* webview : s_activeWebViews) {
        if (webview)
            webview->setWindowlessFrameRate(fps);
    }
}



#endif // USE_CEF 