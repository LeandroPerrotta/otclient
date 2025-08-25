#include "cef_renderer.h"
#include "../ui/uicefwebview.h"
#include <framework/graphics/graphics.h>
#include <framework/graphics/texture.h>

void CefRenderer::draw(Fw::DrawPane drawPane)
{
    if(m_textureCreated && m_cefTexture) {
        Rect rect = m_view.getRect();
        g_painter->setOpacity(1.0);
        g_painter->drawTexturedRect(rect, m_cefTexture);
    }
    (void)drawPane;
}
