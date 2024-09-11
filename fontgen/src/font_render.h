#pragma once

#include <stdint.h>
#include <dmsdk/gamesys/resources/res_font.h>

// Api for Glyph+Bitmap generation

namespace dmFontGen
{
    struct Font;

    uint8_t* GenerateGlyphSdf(Font* font, uint32_t glyph_index,
                                float scale, int padding, int edge,
                                uint32_t dstw, uint32_t dsth, uint32_t dstasc,
                                dmGameSystem::FontGlyph* glyph);
}
