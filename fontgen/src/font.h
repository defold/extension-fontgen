#pragma once

#include <stdint.h>

// Api for dealing with a font (.ttf) resource

namespace dmFontGen
{
    struct Font;

    // Loads a .ttf resource from memory
    Font*       LoadFontFromMem(const char* path, void* resource, uint32_t size);

    // Frees a previously allocated font
    void        DestroyFont(Font* font);

    // Get the path that was assigned to the font
    const char* GetFontPath(Font* font);

    /*
     *
     */
    int CodePointToGlyphIndex(Font* font, int codepoint);

    /*
     * Calculate a scaling value based on the desired font height (in pixels)
     */
    float SizeToScale(Font* font, int size);

    /*
     * Gets the max ascent of the glyphs in the font
     */
    float GetAscent(Font* font, float scale);

    /*
     * Gets the max descent of the glyphs in the font
     */
    float GetDescent(Font* font, float scale);


    // Bitmap generation
}
