#pragma once

#include <GL/gl.h>
#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <chrono>

/**
 * Pipeline GPU Assíncrono com Triple Buffering
 * 
 * Esta classe implementa um pipeline assíncrono que processa frames GPU
 * sem bloquear a thread principal, eliminando a necessidade de thread
 * switching via EventDispatcher para cada frame.
 */
class AsyncGPUPipeline {
public:
    struct FrameData {
        GLuint textureId;
        GLsync fence;
        int fd;
        int width, height;
        int stride, offset;
        uint64_t modifier;
        std::chrono::steady_clock::time_point submitTime;
        std::atomic<bool> ready{false};
        
        FrameData() : textureId(0), fence(nullptr), fd(-1), 
                     width(0), height(0), stride(0), offset(0), modifier(0) {}
    };

    static AsyncGPUPipeline& instance();
    
    // Inicialização e shutdown
    bool initialize();
    void shutdown();
    
    // Interface principal
    void submitFrame(int fd, int width, int height, int stride, int offset, uint64_t modifier);
    GLuint getReadyTexture(); // Non-blocking, retorna 0 se nenhuma textura pronta
    
    // Estatísticas
    struct Stats {
        std::atomic<uint64_t> framesSubmitted{0};
        std::atomic<uint64_t> framesProcessed{0};
        std::atomic<uint64_t> framesDropped{0};
        std::chrono::nanoseconds avgProcessingTime{0};
        std::chrono::nanoseconds maxProcessingTime{0};
    };
    
    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    AsyncGPUPipeline() = default;
    ~AsyncGPUPipeline();
    
    // Não copiável
    AsyncGPUPipeline(const AsyncGPUPipeline&) = delete;
    AsyncGPUPipeline& operator=(const AsyncGPUPipeline&) = delete;
    
    // Worker thread
    void workerThreadMain();
    void processFrame(FrameData& frame);
    
    // Triple buffering
    static constexpr size_t BUFFER_COUNT = 3;
    std::array<FrameData, BUFFER_COUNT> m_frameBuffer;
    std::atomic<int> m_writeIndex{0};
    std::atomic<int> m_readIndex{0};
    std::atomic<int> m_processIndex{0};
    
    // Thread management
    std::thread m_workerThread;
    std::atomic<bool> m_running{false};
    std::mutex m_workMutex;
    std::condition_variable m_workCondition;
    std::queue<int> m_workQueue;
    
    // Performance tracking
    Stats m_stats;
    mutable std::mutex m_statsMutex;
    
    bool m_initialized{false};
};