/*
 * Copyright (c) 2010-2012 OTClient <https://github.com/edubart/otclient>
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

#include "texturemanager.h"
#include "animatedtexture.h"
#include "graphics.h"
#include "image.h"

#include <framework/core/resourcemanager.h>
#include <framework/thirdparty/apngloader.h>

TextureManager g_textures;

void TextureManager::init()
{
    m_emptyTexture = TexturePtr(new Texture);
}

void TextureManager::terminate()
{
#ifndef NDEBUG
    // check for leaks
    int refs = 0;
    for(const auto& it : m_textures)
        if(it.second.use_count() > 1)
            refs++;
    if(refs > 0)
        g_logger.debug(stdext::format("%d textures references left", refs));
#endif
    m_textures.clear();
    m_emptyTexture = nullptr;
}

TexturePtr TextureManager::getTexture(const std::string& fileName)
{
    TexturePtr texture;

    // before must resolve filename to full path
    std::string filePath = g_resources.resolvePath(fileName);

    // check if the texture is already loaded
    auto it = m_textures.find(filePath);
    if(it != m_textures.end()) {
        if(it->second.expired())
            m_textures.erase(it);
        else
            texture = it->second.lock();
    }

    // texture not found, load it
    if(!texture) {
        try {
            // currently only png textures are supported
            if(!boost::ends_with(filePath, ".png"))
                stdext::throw_exception("texture file format no supported");

            // load texture file data
            std::stringstream fin;
            g_resources.loadFile(filePath, fin);
            texture = loadPNG(fin);
        } catch(stdext::exception& e) {
            g_logger.error(stdext::format("unable to load texture '%s': %s", fileName, e.what()));
            texture = g_textures.getEmptyTexture();
        }

        if(texture)
            m_textures[filePath] = texture;
    }

    return texture;
}

TexturePtr TextureManager::loadPNG(std::stringstream& file)
{
    TexturePtr texture;

    apng_data apng;
    if(load_apng(file, &apng) == 0) {
        if(apng.num_frames > 1) { // animated texture
            //uchar *framesdata = apng.pdata + (apng.first_frame * apng.width * apng.height * apng.bpp);
            //texture = TexturePtr(new AnimatedTexture(apng.width, apng.height, apng.bpp, apng.num_frames, framesdata, (int*)apng.frames_delay));
            g_logger.error("animated textures is disabled for a while");
        } else {
            ImagePtr image = ImagePtr(new Image(Size(apng.width, apng.height), apng.bpp, apng.pdata));
            texture = TexturePtr(new Texture(image));
        }
        free_apng(&apng);
    }

    return texture;
}
