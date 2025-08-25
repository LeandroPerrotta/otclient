#include "cef_renderer_cpu.h"
#include "../ui/uicefwebview.h"
#include <framework/graphics/graphics.h>
#include <framework/core/logger.h>
#include <vector>
#include <cstring>

CefRendererCPU::CefRendererCPU(UICEFWebView& view)
    : CefRenderer(view)
    , m_cefTexture(nullptr)
    , m_cefImage(nullptr)
    , m_textureCreated(false)
    , m_lastWidth(0)
    , m_lastHeight(0)
{
}

void CefRendererCPU::onPaint(const void* buffer, int width, int height,
                             const CefRenderHandler::RectList& dirtyRects)
{
    if(!buffer || width <= 0 || height <= 0)
        return;

    const bool useBGRA = g_graphics.canUseBGRA();

    if(!m_cefImage || m_cefImage->getSize() != Size(width, height)) {
        m_cefImage = ImagePtr(new Image(Size(width, height), 4));
        m_textureCreated = false;
    }

    const uint8_t* bgra = static_cast<const uint8_t*>(buffer);
    uint8_t* dest = m_cefImage->getPixelData();

    auto copyRect = [&](int x, int y, int w, int h) {
        for(int row = 0; row < h; ++row) {
            const uint8_t* srcRow = bgra + ((y + row) * width + x) * 4;
            uint8_t* dstRow = dest + ((y + row) * width + x) * 4;

            if(useBGRA) {
                memcpy(dstRow, srcRow, w * 4);
            } else {
                for(int col = 0; col < w; ++col) {
                    dstRow[col*4 + 0] = srcRow[col*4 + 2];
                    dstRow[col*4 + 1] = srcRow[col*4 + 1];
                    dstRow[col*4 + 2] = srcRow[col*4 + 0];
                    dstRow[col*4 + 3] = srcRow[col*4 + 3];
                }
            }
        }
    };

    if(dirtyRects.empty()) {
        copyRect(0, 0, width, height);
    } else {
        for(const auto& r : dirtyRects) {
            copyRect(r.x, r.y, r.width, r.height);
        }
    }

    if(!m_textureCreated || width != m_lastWidth || height != m_lastHeight) {
        m_cefTexture = TexturePtr(new Texture(m_cefImage, false, false, useBGRA));
        m_textureCreated = true;
        m_lastWidth = width;
        m_lastHeight = height;
    } else {
        if(dirtyRects.empty()) {
            Rect full(0, 0, width, height);
            m_cefTexture->updateSubPixels(full, dest, 4, useBGRA);
        } else {
            for(const auto& r : dirtyRects) {
                Rect upRect(r.x, r.y, r.width, r.height);
                std::vector<uint8_t> temp(r.width * r.height * 4);
                const size_t bytesPerRow = r.width * 4;
                for(int row = 0; row < r.height; ++row) {
                    const uint8_t* srcRow = dest + ((r.y + row) * width + r.x) * 4;
                    uint8_t* dstRow = temp.data() + row * bytesPerRow;
                    memcpy(dstRow, srcRow, bytesPerRow);
                }
                m_cefTexture->updateSubPixels(upRect, temp.data(), 4, useBGRA);
            }
        }
    }

    m_view.setVisible(true);
}

void CefRendererCPU::onAcceleratedPaint(const CefAcceleratedPaintInfo& info)
{
    (void)info;
}

void CefRendererCPU::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    }
}
