#include "font_render.h"
#include "font_private.h"

#include <dmsdk/dlib/log.h>

#include "stb_truetype.h"

namespace dmFontGen {


// From atlaspacker.c (with small fix to avoid rotation)
static void apCopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                        const uint8_t* source, int source_width, int source_height, int source_channels,
                        int dest_x, int dest_y)
{
    for (int sy = 0; sy < source_height; ++sy)
    {
        for (int sx = 0; sx < source_width; ++sx, source += source_channels)
        {
            // Skip copying texels with alpha=0
            if (source_channels == 4 && source[3] == 0)
                continue;

            // Map the current coord into the rotated space
            //apPos rotated_pos = apRotate(sx, sy, source_width, source_height, rotation);

            // int target_x = dest_x + rotated_pos.x;
            // int target_y = dest_y + rotated_pos.y;

            int target_x = dest_x + sx;
            int target_y = dest_y + sy;

            // If the target is outside of the destination area, we skip it
            if (target_x < 0 || target_x >= dest_width ||
                target_y < 0 || target_y >= dest_height)
                continue;

            int dest_index = target_y * dest_width * dest_channels + target_x * dest_channels;

            uint8_t color[4] = {255,255,255,255};
            for (int c = 0; c < source_channels; ++c)
                color[c] = source[c];

            // int alphathreshold = 8;
            // if (alphathreshold >= 0 && color[3] <= alphathreshold)
            //     continue; // Skip texels that are <= the alpha threshold

            for (int c = 0; c < dest_channels; ++c)
                dest[dest_index+c] = color[c];

            // if (color[3] > 0 && color[3] < 255)
            // {
            //     uint32_t r = dest[dest_index+0] + 48;
            //     dest[dest_index+0] = (uint8_t)(r > 255 ? 255 : r);
            //     dest[dest_index+1] = dest[dest_index+1] / 2;
            //     dest[dest_index+2] = dest[dest_index+2] / 2;

            //     uint32_t a = dest[dest_index+3] + 128;
            //     dest[dest_index+3] = (uint8_t)(a > 255 ? 255 : a);
            // }
        }
    }
}

/*
 * @param glyph_index [type: int] the glyph index to be rendered
 * @param scale [type: float] the scale to use
 * @param padding [type: int] the number of pixels the glyph should be expanded with
 *                            Mostly mapping to our "outline width". See "padding" in stb_truetype.h for full info
 * @param edge [type: int] the edge value [0-255]
 *                         Where the edge is decided to be [0-255]. See "onedge_value" in stb_truetype.h for full info
 * @param dstw [type: int] the width of the glyph cell image
 * @param dsth [type: int] the height of the glyph cell image
 * @param dst [type: uint8_t*] where the generated sdf image (dst*dsth in size) [out]
 * @param glyph [type: dmGameSystem::FontGlyph*] the glyph values value [out]
 * @return result [type: bool] Returns ok if siccessful
 */
static bool RenderGlyphSDF(Font* font, int glyph_index, float scale, int padding, int edge,
                    int dstw, int dsth, int dstasc, uint8_t* dst, dmGameSystem::FontGlyph* out)
{
    float pixel_dist_scale = (float)edge/(float)padding;

    int srcw, srch, offsetx, offsety;
    uint8_t* src = stbtt_GetGlyphSDF(&font->m_Font, scale, glyph_index, padding, edge, pixel_dist_scale,
                                        &srcw, &srch, &offsetx, &offsety);
    if (!src)
    {
        dmLogError("Glyph index %d does not exist in font: %s", glyph_index, font->m_Path);
        return false;
    }

    int advx, lsb;
    stbtt_GetGlyphHMetrics(&font->m_Font, glyph_index, &advx, &lsb);

    // We need to make sure the base lines match up
    // We blit the generated image into the cache cell
    int x = 0;
    int y = dstasc + offsety;
    apCopyRGBA( dst, dstw, dsth, 1,
                src, srcw, srch, 1,
                x, y);

    stbtt_FreeSDF(src, 0);

    out->m_Width = dstw;
    out->m_Height = dsth;
    out->m_Advance = advx*scale;;
    out->m_LeftBearing = lsb*scale;;
    out->m_Ascent = font->m_Ascent*scale;
    out->m_Descent = font->m_Descent*scale;

    //printf("glyph: w/h: %d, %d off: %d, %d  adv: %.2f  lsb: %.2f\n", glyph->m_Width, glyph->m_Height, glyph->m_OffsetX, glyph->m_OffsetY, glyph->m_Advance, glyph->m_LeftBearing);
    // DebugPrintBitmap(glyph->m_Data, glyph->m_Width, glyph->m_Height);
    return true;
}


uint8_t* GenerateGlyphSdf(Font* font, uint32_t glyph_index,
                            float scale, int padding, int edge,
                            uint32_t dstw, uint32_t dsth, uint32_t dstasc,
                            dmGameSystem::FontGlyph* glyph)
{
    uint32_t memsize = dstw*dsth + 1;
    uint8_t* mem = (uint8_t*)malloc(memsize);
    memset(mem, 0, memsize);
    uint8_t* bm = mem + 1;

    mem[0] = 0; // 0: no compression, 1: deflate

    if (!RenderGlyphSDF(font, glyph_index, scale, padding, edge, dstw, dsth, dstasc, bm, glyph))
    {
        delete mem;
        return 0;
    }

    return mem;
}

} // namespace
