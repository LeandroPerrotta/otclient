#pragma once

#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <vector>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <atomic>

/**
 * GPU Texture Pool para implementação zero-copy real
 * 
 * Esta classe gerencia um pool de texturas OpenGL reutilizáveis que são
 * vinculadas diretamente aos DMA buffers do CEF via EGLImage, eliminando
 * a necessidade de recriar texturas a cada frame.
 */
class GPUTexturePool {
public:
    struct TextureHandle {
        GLuint textureId;
        EGLImageKHR eglImage;
        int width, height;
        uint64_t modifier;
        std::chrono::steady_clock::time_point lastUsed;
        std::atomic<bool> inUse{false};
        
        TextureHandle() : textureId(0), eglImage(EGL_NO_IMAGE_KHR), 
                         width(0), height(0), modifier(0) {}
    };

    static GPUTexturePool& instance();
    
    // Inicialização e cleanup
    bool initialize(EGLDisplay eglDisplay);
    void shutdown();
    
    // Gerenciamento de texturas
    TextureHandle* acquireTexture(int width, int height, int fd, 
                                 int stride, int offset, uint64_t modifier);
    void releaseTexture(TextureHandle* handle);
    
    // Manutenção do pool
    void cleanup(); // Remove texturas não usadas
    size_t getPoolSize() const { return m_texturePool.size(); }
    size_t getActiveCount() const { return m_activeCount.load(); }

private:
    GPUTexturePool() = default;
    ~GPUTexturePool();
    
    // Não copiável
    GPUTexturePool(const GPUTexturePool&) = delete;
    GPUTexturePool& operator=(const GPUTexturePool&) = delete;
    
    // Métodos internos
    TextureHandle* createNewTexture(int width, int height, int fd,
                                   int stride, int offset, uint64_t modifier);
    bool bindDMABuffer(TextureHandle* handle, int fd, int stride, 
                      int offset, uint64_t modifier);
    void destroyTexture(TextureHandle* handle);
    
    // Estado interno
    std::vector<std::unique_ptr<TextureHandle>> m_texturePool;
    mutable std::mutex m_poolMutex;
    
    EGLDisplay m_eglDisplay{EGL_NO_DISPLAY};
    std::atomic<size_t> m_activeCount{0};
    
    // Configuração
    static constexpr size_t MAX_POOL_SIZE = 16;
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{5};
    
    // Função pointers para EGL
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR{nullptr};
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR{nullptr};
    
    bool m_initialized{false};
};