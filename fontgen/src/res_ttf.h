#pragma once

#include <stdint.h>
#include <dmsdk/gamesys/resources/res_font.h>

namespace dmFontGen
{
    struct TTFResource;

    const char* GetFontPath(TTFResource* resource);

    /*
     *
     */
    int CodePointToGlyphIndex(TTFResource* resource, int codepoint);

    /*
     * Calculate a scaling value based on the desired font height (in pixels)
     */
    float SizeToScale(TTFResource* resource, int size);

    /*
     * Gets the max ascent of the glyphs in the font
     */
    float GetAscent(TTFResource* resource, float scale);

    /*
     * Gets the max descent of the glyphs in the font
     */
    float GetDescent(TTFResource* resource, float scale);

    /*
     *
     */
    uint8_t* GenerateGlyphSdf(TTFResource* font, uint32_t glyph_index,
                            float scale, int padding, int edge,
                            dmGameSystem::FontGlyph* glyph);
}
