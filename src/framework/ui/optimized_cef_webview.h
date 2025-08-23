#pragma once

#include "uiwebview.h"
#include "gpu_texture_pool.h"
#include "async_gpu_pipeline.h"
#include <mutex>
#include <atomic>
#include <chrono>

#ifdef USE_CEF
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include <framework/graphics/texture.h>
#include <framework/graphics/image.h>

// Forward declaration
class OptimizedCEFClient;
#endif

/**
 * Versão Otimizada do UICEFWebView
 * 
 * Esta classe implementa todas as otimizações propostas:
 * - Texture Pool para zero-copy real
 * - Pipeline GPU assíncrono
 * - Cache de contexto OpenGL
 * - Mouse event batching
 * - Detecção inteligente de capacidades
 */
class OptimizedCEFWebView : public UIWebView {
public:
    OptimizedCEFWebView();
    OptimizedCEFWebView(UIWidgetPtr parent);
    virtual ~OptimizedCEFWebView();

    // CEF-specific methods (otimizados)
    void onCEFAcceleratedPaintOptimized(const CefAcceleratedPaintInfo& info);
    void onBrowserCreated(CefRefPtr<CefBrowser> browser);
    
    // Static methods for managing all WebViews
    static void closeAllWebViews();
    static size_t getActiveWebViewCount();
    static void setAllWindowlessFrameRate(int fps);

    void setWindowlessFrameRate(int fps);

    // Event handlers
    void onLoadStarted() override;
    void onLoadFinished(bool success) override;
    void onUrlChanged(const std::string& url) override;
    void onTitleChanged(const std::string& title) override;
    void onJavaScriptCallback(const std::string& name, const std::string& data) override;

    // Optimized mouse input handlers
    bool onMousePress(const Point& mousePos, Fw::MouseButton button) override;
    bool onMouseRelease(const Point& mousePos, Fw::MouseButton button) override;
    bool onMouseMove(const Point& mousePos, const Point& mouseMoved) override;
    bool onMouseWheel(const Point& mousePos, Fw::MouseWheelDirection direction) override;
    void onHoverChange(bool hovered) override;

    // Keyboard input handlers
    bool onKeyText(const std::string& keyText) override;
    bool onKeyDown(uchar keyCode, int keyboardModifiers) override;
    bool onKeyPress(uchar keyCode, int keyboardModifiers, int autoRepeatTicks) override;
    bool onKeyUp(uchar keyCode, int keyboardModifiers) override;    

    // Geometry change handler (otimizado)
    void onGeometryChange(const Rect& oldRect, const Rect& newRect) override;

    // Performance metrics
    struct PerformanceMetrics {
        std::chrono::nanoseconds avgFrameLatency{0};
        std::chrono::nanoseconds maxFrameLatency{0};
        size_t gpuMemoryUsage{0};
        float cpuUsagePercent{0.0f};
        int droppedFrames{0};
        int mouseEventLatency{0};
        uint64_t totalFrames{0};
    };
    
    const PerformanceMetrics& getMetrics() const { return m_metrics; }
    void resetMetrics();

protected:
    void createWebView() override;
    void loadUrlInternal(const std::string& url) override;
    bool loadHtmlInternal(const std::string& html, const std::string& baseUrl) override;
    void executeJavaScriptInternal(const std::string& script) override;
    void drawSelf(Fw::DrawPane drawPane) override;

private:
#ifdef USE_CEF
    friend class OptimizedCEFClient;
    CefRefPtr<CefBrowser> m_browser;
    CefRefPtr<CefClient> m_client;
    std::string m_pendingHtml;
    std::string m_pendingUrl;
    
    // Optimized texture management
    GPUTexturePool::TextureHandle* m_currentTexture;
    std::atomic<bool> m_textureReady{false};
    
    // Mouse event batching
    struct MouseEvent {
        CefMouseEvent event;
        CefBrowserHost::MouseButtonType buttonType;
        bool isPress;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    std::vector<MouseEvent> m_pendingMouseEvents;
    std::mutex m_mouseEventMutex;
    std::chrono::steady_clock::time_point m_lastMouseFlush;
    
    // GPU capabilities detection (static, shared entre instâncias)
    struct GPUCapabilities {
        static std::atomic<bool> s_detected;
        static bool s_memoryObjectSupported;
        static bool s_eglImageSupported;
        static bool s_dmaBufferSupported;
        
        static void detect();
        static bool useMemoryObject() { return s_memoryObjectSupported; }
        static bool useEGLImage() { return s_eglImageSupported; }
        static bool useDMABuffer() { return s_dmaBufferSupported; }
    };
    
    // OpenGL context optimization
    class OptimizedGLContext {
    private:
        static thread_local bool s_contextValidated;
        static thread_local void* s_lastContext;
        static thread_local std::chrono::steady_clock::time_point s_lastValidation;
        
    public:
        static bool ensureContext();
        static void invalidateCache();
    };
    
    // Performance tracking
    mutable PerformanceMetrics m_metrics;
    mutable std::mutex m_metricsMutex;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    
    // Static tracking of all active WebViews (thread-safe)
    static std::vector<OptimizedCEFWebView*> s_activeWebViews;
    static std::mutex s_activeWebViewsMutex;
    
    // Instance validity flag
    std::atomic<bool> m_isValid{true};
    
    // Optimized methods
    void processAcceleratedPaintOptimized(const CefAcceleratedPaintInfo& info);
    void flushMouseEvents();
    void updateMetrics(std::chrono::nanoseconds frameTime);
    
    // Initialization helpers
    static void initializeGPUCapabilities();
    static void initializeOptimizedPipeline();
    static void cleanupOptimizedResources();
#endif
};