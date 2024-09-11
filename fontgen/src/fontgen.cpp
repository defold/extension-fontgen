#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/utf8.h>

#include <dmsdk/gamesys/resources/res_font.h>

#include "font.h"
#include "font_render.h"
#include "fontgen.h"

namespace dmFontGen
{

const char* path = "/assets/fonts/vero.fontc";
const char* data_path = "/assets/fonts/vera_mo_bd.ttf";
//const char* path = "/assets/fonts/vera_mo_bd.ttf";


struct FontData // the ttf data
{
    dmFontGen::Font* m_Font;
    int              m_RefCount;
    void*            m_Resource; // Needs to be alive for the stb font
};

struct FontInfo
{
    dmGameSystem::FontResource* m_FontResource;
    FontData*                   m_FontData;
    int                         m_Padding;
    int                         m_EdgeValue;
    float                       m_Scale;
};

struct Context
{
    HResourceFactory            m_ResourceFactory;
    dmHashTable64<FontData*>    m_FontDatas; // Loaded .ttf files
    dmHashTable64<FontInfo*>    m_FontInfos; // Loaded .fontc files

    uint8_t                     m_DefaultSdfPadding;
    uint8_t                     m_DefaultSdfEdge;
};

Context* g_FontExtContext = 0;

static void UnloadFontData(Context* ctx, dmhash_t hash, FontData* data)
{
    if ((--data->m_RefCount) == 0)
    {
        dmFontGen::DestroyFont(data->m_Font);
        data->m_Font = 0;
        delete data;
        ctx->m_FontDatas.Erase(hash);
    }
}

static FontData* LoadFontData(Context* ctx, const char* path)
{
    dmhash_t path_hash = dmHashString64(path);

    FontData** datap = ctx->m_FontDatas.Get(path_hash);
    if (datap)
    {
        (*datap)->m_RefCount++;
        return *datap;
    }

    // Get the meta data from the engine resource
    uint32_t    resource_size = 0;
    void*       resource = 0;
    dmResource::Result r = dmResource::GetRaw(ctx->m_ResourceFactory, path, &resource, &resource_size);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get font data from '%s'", path);
        return 0;
    }


    dmFontGen::Font* font = dmFontGen::LoadFontFromMem(path, resource, resource_size);

    if (font == 0)
    {
        free(resource);
        dmLogError("Failed to parse font '%s'", path);
        return 0;
    }

    FontData* data = new FontData;
    data->m_RefCount = 0;
    data->m_Font = font;
    data->m_Resource = resource;

    if (ctx->m_FontDatas.Full())
    {
        uint32_t cap = ctx->m_FontDatas.Capacity() + 8;
        ctx->m_FontDatas.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_FontDatas.Put(path_hash, data);

    dmLogInfo("Loaded font data'%s", path);
    return data;
}


static FontInfo* LoadFont(Context* ctx, const char* path)
{
    // Get the meta data from the engine resource
    dmhash_t path_hash = dmHashString64(path);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::Get(ctx->m_ResourceFactory, path, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get resource '%s'", path);
        return 0;
    }

    dmGameSystem::FontMapDesc font_desc;
    r = dmGameSystem::ResFontGetInfo(resource, &font_desc);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get font info from '%s'", path);
        return 0;
    }

    //const char* font_data_path = font_desc.m_Font;
    const char* font_data_path = data_path;

    FontData* data = LoadFontData(ctx, font_data_path);
    if (!data)
    {
        dmResource::Release(ctx->m_ResourceFactory, resource);
        return 0;
    }


    FontInfo* info = new FontInfo;

    info->m_FontResource = resource;
    info->m_FontData     = data;
    info->m_Padding      = ctx->m_DefaultSdfPadding + font_desc.m_ShadowBlur + font_desc.m_OutlineWidth; // 3 is arbitrary but resembles the output from out generator
    info->m_EdgeValue    = ctx->m_DefaultSdfEdge;
    info->m_Scale        = dmFontGen::SizeToScale(data->m_Font, font_desc.m_Size);

    // printf("FONT: %s\n", path);
    // printf("  ttf:     %s\n", font_data_path);
    // printf("  size:    %u\n", font_desc.m_Size);
    // printf("  padding: %d\n", info->m_Padding);
    // printf("  edge:    %d\n", info->m_EdgeValue );
    // printf("  scale:   %.3f\n", info->m_Scale);
    // printf("  ascent:  %.3f\n", dmFontGen::GetAscent(data->m_Font, info->m_Scale));
    // printf("  descent: %.3f\n", dmFontGen::GetDescent(data->m_Font, info->m_Scale));

    if (ctx->m_FontInfos.Full())
    {
        uint32_t cap = ctx->m_FontInfos.Capacity() + 8;
        ctx->m_FontInfos.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_FontInfos.Put(path_hash, info);

    dmLogInfo("Loaded font '%s", path);
    return info;
}

static void DeleteFontDataIter(Context* ctx, const dmhash_t* hash, FontData** datap)
{
    (void)ctx;
    (void)hash;
    FontData* data = *datap;

    dmLogWarning("Font data wasn't unloaded: %s", dmFontGen::GetFontPath(data->m_Font));

    dmFontGen::DestroyFont(data->m_Font);
    free(data->m_Resource);
    delete data;
}

static void DeleteFontInfoIter(Context* ctx, const dmhash_t* hash, FontInfo** infop)
{
    (void)hash;
    FontInfo* info = *infop;

    dmhash_t path_hash;
    dmResource::GetPath(ctx->m_ResourceFactory, info->m_FontResource, &path_hash);
    dmLogWarning("Font resource wasn't released: %s", dmHashReverseSafe64(path_hash));

    dmResource::Release(ctx->m_ResourceFactory, info->m_FontResource);
    delete info;
}


// NOTE:
//   Currently unsupported features:
//      * Bitmap fonts
//      * Caching the generated data to disc

static void AddGlyphs(FontInfo* info, const char* text)
{
    uint32_t cache_cell_width;
    uint32_t cache_cell_height;
    uint32_t cache_cell_max_ascent;
    dmResource::Result r = dmGameSystem::ResFontGetCacheCellInfo(info->m_FontResource, &cache_cell_width, &cache_cell_height, &cache_cell_max_ascent);

    dmFontGen::Font* font = info->m_FontData->m_Font;

    float scale = info->m_Scale;
    float ascent = dmFontGen::GetAscent(font, scale);
    float descent = dmFontGen::GetDescent(font, scale);
    bool is_sdf = true;

    const char* cursor = text;
    uint32_t c = 0;

    while ((c = dmUtf8::NextChar(&cursor)))
    {
        //uint32_t c = chars[i];
        printf("---------------------------------------------\n");
        printf("CHAR: '%c'\n", c);

        if (dmGameSystem::ResFontHasGlyph(info->m_FontResource, c))
        {
            printf("  already existed: '%c'\n", c);
            continue;
        }

        uint32_t glyph_index = dmFontGen::CodePointToGlyphIndex(font, c);
        if (!glyph_index)
            continue; // No such glyph!

        uint32_t celldatasize = cache_cell_width * cache_cell_height + 1;
        uint8_t* celldata = 0;

        dmGameSystem::FontGlyph fg;

        // Blit the font glyph image into a cache cell image
        if (is_sdf)
            celldata = dmFontGen::GenerateGlyphSdf(font, glyph_index, scale, info->m_Padding, info->m_EdgeValue,
                                                    cache_cell_width, cache_cell_height, cache_cell_max_ascent,
                                                    &fg);

        if (!celldata)
            continue; // Something went wrong

        // The font system takes ownership of the image data
        dmResource::Result r = dmGameSystem::ResFontAddGlyph(info->m_FontResource, c, &fg, celldata, celldatasize);
    }
}

static dmExtension::Result OnUpdateFontGen(dmExtension::Params* params)
{
    static int count = 0;
    if (count++ == 20)
    {
        FontInfo* info = LoadFont(g_FontExtContext, path);

        if (!info)
            return dmExtension::RESULT_OK;

        const char* text = "DEFdef";
        AddGlyphs(info, text);

        ResFontDebugPrint(info->m_FontResource);
    }

    return dmExtension::RESULT_OK;
}

bool Initialize(dmExtension::Params* params)
{
    g_FontExtContext = new Context;
    g_FontExtContext->m_ResourceFactory = params->m_ResourceFactory;

    g_FontExtContext->m_DefaultSdfPadding = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.sdf_base_padding", 3);
    g_FontExtContext->m_DefaultSdfEdge = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.sdf_edge_value", 190);
    return true;
}

void Finalize(dmExtension::Params* params)
{
    g_FontExtContext->m_FontInfos.Iterate(DeleteFontInfoIter, g_FontExtContext);
    g_FontExtContext->m_FontInfos.Clear();
    g_FontExtContext->m_FontDatas.Iterate(DeleteFontDataIter, g_FontExtContext);
    g_FontExtContext->m_FontDatas.Clear();

    delete g_FontExtContext;
    g_FontExtContext = 0;
}


void Update(dmExtension::Params* params)
{
    // TODO: Add worker thread!
    // TODO: Loop over any finished items

    static int count = 0;
    if (count++ == 20)
    {
        FontInfo* info = LoadFont(g_FontExtContext, path);

        if (!info)
            return;

        const char* text = "DEFdef";
        AddGlyphs(info, text);

        ResFontDebugPrint(info->m_FontResource);
    }
}

} // namespace
