#include "font.h"
#include "font_private.h"
#include "util.h"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

namespace dmFontGen {

Font* LoadFontFromMem(const char* path, void* resource, uint32_t size)
{
    Font* font = new Font;
    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)resource,0);
    int result = stbtt_InitFont(&font->m_Font, (const unsigned char*)resource, index);
    if (!result)
    {
        delete font;
        return 0;
    }

    stbtt_GetFontVMetrics(&font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    //printf("stbtt_GetFontVMetrics: asc: %d  dsc: %d  lg: %d\n", font->m_Ascent, font->m_Descent, font->m_LineGap);
    return font;
}

void DestroyFont(Font* font)
{
    (void)font;
    free((void*)font->m_Path);
    delete font;
}

const char* GetFontPath(Font* font)
{
    return font->m_Path;
}

int CodePointToGlyphIndex(Font* font, int codepoint)
{
    return stbtt_FindGlyphIndex(&font->m_Font, codepoint);
}

float SizeToScale(Font* font, int size)
{
    return stbtt_ScaleForPixelHeight(&font->m_Font, size);
}

float GetAscent(Font* font, float scale)
{
    return font->m_Ascent * scale;
}

float GetDescent(Font* font, float scale)
{
    return font->m_Descent * scale;
}

} // namespace
