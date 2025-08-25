#pragma once

#include "../cef_renderer.h"
#include "gpu_helper.h"
#include <framework/graphics/texture.h>

#if defined(USE_CEF) && defined(__linux__)
#include <GL/gl.h>

#ifndef GL_EXT_memory_object_fd
#define GL_EXT_memory_object_fd 1
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
typedef void (APIENTRYP PFNGLCREATEMEMORYOBJECTSEXTPROC)(GLsizei n, GLuint* memoryObjects);
typedef void (APIENTRYP PFNGLIMPORTMEMORYFDEXTPROC)(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
typedef void (APIENTRYP PFNGLTEXSTORAGEMEM2DEXTPROC)(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
typedef void (APIENTRYP PFNGLDELETEMEMORYOBJECTSEXTPROC)(GLsizei n, const GLuint* memoryObjects);
#endif
#endif

class CefRendererGPULinuxMesa : public CefRenderer
{
public:
    explicit CefRendererGPULinuxMesa(UICEFWebView& view);
    void onPaint(const void* buffer, int width, int height,
                 const CefRenderHandler::RectList& dirtyRects) override;
    void onAcceleratedPaint(const CefAcceleratedPaintInfo& info) override;
    void draw(Fw::DrawPane drawPane) override;
    bool isSupported() const override;

private:
    TexturePtr m_cefTexture;
    bool m_textureCreated;
    int m_lastWidth;
    int m_lastHeight;
    mutable bool m_checkedSupport;
    mutable bool m_supported;
    mutable PFNGLCREATEMEMORYOBJECTSEXTPROC m_glCreateMemoryObjectsEXT;
    mutable PFNGLIMPORTMEMORYFDEXTPROC m_glImportMemoryFdEXT;
    mutable PFNGLTEXSTORAGEMEM2DEXTPROC m_glTexStorageMem2DEXT;
    mutable PFNGLDELETEMEMORYOBJECTSEXTPROC m_glDeleteMemoryObjectsEXT;
};
