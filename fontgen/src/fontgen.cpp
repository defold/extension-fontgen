#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/utf8.h>

#include <dmsdk/gamesys/resources/res_font.h>

#include "res_ttf.h"
#include "fontgen.h"

namespace dmFontGen
{

struct FontInfo
{
    dmGameSystem::FontResource* m_FontResource;
    dmFontGen::TTFResource*     m_TTFResource;
    int                         m_Padding;
    int                         m_EdgeValue;
    float                       m_Scale;
};

struct Context
{
    HResourceFactory            m_ResourceFactory;
    dmHashTable64<FontInfo*>    m_FontInfos; // Loaded .fontc files

    uint8_t                     m_DefaultSdfPadding;
    uint8_t                     m_DefaultSdfEdge;
};

Context* g_FontExtContext = 0;

static bool CheckType(HResourceFactory factory, const char* path, const char** types, uint32_t num_types)
{
    HResourceDescriptor rd;
    dmResource::Result r = dmResource::GetDescriptor(factory, path, &rd);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get descriptor for resource %s", path);
        return 0;
    }

    HResourceType type = ResourceDescriptorGetType(rd);
    const char* type_name = ResourceTypeGetName(type);

    for (uint32_t i = 0; i < num_types; ++i)
    {
dmLogWarning("COMPARING %s == %s", type_name, types[i]);
        if (strcmp(type_name, types[i]) == 0)
            return true;
    }
    return false;
}

static TTFResource* LoadFontData(Context* ctx, const char* path)
{
    TTFResource* resource = 0;
    dmResource::Result r = dmResource::Get(ctx->m_ResourceFactory, path, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get resource '%s'", path);
        //DeleteFontData(ctx, data);
        return 0;
    }

    const char* types[] = { "ttf" };
    if (!CheckType(ctx->m_ResourceFactory, path, types, 1))
    {
        dmLogError("Wrong type of resource %s (expected %s)", path, types[0]);
        dmResource::Release(ctx->m_ResourceFactory, resource);
        //DeleteFontData(ctx, data);
        return 0;
    }

    dmLogInfo("Loaded font data'%s", path);
    return resource;
}

static void DeleteFont(Context* ctx, FontInfo* info)
{
    if (info->m_FontResource)
        dmResource::Release(ctx->m_ResourceFactory, info->m_FontResource);
    if (info->m_TTFResource)
        dmResource::Release(ctx->m_ResourceFactory, info->m_TTFResource);
    delete info;
}

static bool UnloadFont(Context* ctx, dmhash_t fontc_path_hash)
{
    FontInfo** ppinfo = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!ppinfo)
    {
        dmLogError("Font not loaded: %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    DeleteFont(ctx, *ppinfo);
    ctx->m_FontInfos.Erase(fontc_path_hash);
    return true;
}

static FontInfo* LoadFont(Context* ctx, const char* fontc_path, const char* ttf_path)
{
    dmhash_t path_hash = dmHashString64(fontc_path);

    FontInfo* info = new FontInfo;
    memset(info, 0, sizeof(*info));

    dmResource::Result r = dmResource::Get(ctx->m_ResourceFactory, fontc_path, (void**)&info->m_FontResource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get .fontc resource '%s'", fontc_path);
        DeleteFont(ctx, info);
        return 0;
    }

    const char* types[] = { "fontc" };
    if (!CheckType(ctx->m_ResourceFactory, fontc_path, types, 1))
    {
        DeleteFont(ctx, info);
        return 0;
    }

    info->m_TTFResource = LoadFontData(ctx, ttf_path);
    if (!info->m_TTFResource)
    {
        DeleteFont(ctx, info);
        return 0;
    }

    dmGameSystem::FontInfo font_info;
    r = dmGameSystem::ResFontGetInfo(info->m_FontResource, &font_info);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get font info from '%s'", fontc_path);
        return 0;
    }

    info->m_Padding      = ctx->m_DefaultSdfPadding + font_info.m_ShadowBlur + font_info.m_OutlineWidth; // 3 is arbitrary but resembles the output from out generator
    info->m_EdgeValue    = ctx->m_DefaultSdfEdge;
    info->m_Scale        = dmFontGen::SizeToScale(info->m_TTFResource, font_info.m_Size);

    uint32_t cell_width = 0;
    uint32_t cell_height = 0;
    uint32_t max_ascent = 0;
    dmFontGen::GetCellSize(info->m_TTFResource, &cell_width, &cell_height, &max_ascent);

    r = ResFontSetCacheCellSize(info->m_FontResource, cell_width*info->m_Scale, cell_height*info->m_Scale, info->m_Padding + max_ascent*info->m_Scale);

    if (ctx->m_FontInfos.Full())
    {
        uint32_t cap = ctx->m_FontInfos.Capacity() + 8;
        ctx->m_FontInfos.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_FontInfos.Put(path_hash, info);

    dmLogInfo("Loaded font '%s", fontc_path);
    return info;
}

static void DeleteFontInfoIter(Context* ctx, const dmhash_t* hash, FontInfo** infop)
{
    (void)hash;
    FontInfo* info = *infop;

    dmhash_t path_hash;
    dmResource::GetPath(ctx->m_ResourceFactory, info->m_FontResource, &path_hash);
    dmLogWarning("Font resource wasn't released: %s", dmHashReverseSafe64(path_hash));

    dmResource::Release(ctx->m_ResourceFactory, info->m_FontResource);
    dmResource::Release(ctx->m_ResourceFactory, info->m_TTFResource);
    delete info;
}

static void AddGlyphs(FontInfo* info, const char* text)
{
    uint64_t tstart = dmTime::GetTime();

    uint32_t cache_cell_width;
    uint32_t cache_cell_height;
    uint32_t cache_cell_max_ascent;
    dmResource::Result r = dmGameSystem::ResFontGetCacheCellSize(info->m_FontResource, &cache_cell_width, &cache_cell_height, &cache_cell_max_ascent);

    TTFResource* ttfresource = info->m_TTFResource;

    float scale = info->m_Scale;
    float ascent = dmFontGen::GetAscent(ttfresource, scale);
    float descent = dmFontGen::GetDescent(ttfresource, scale);
    bool is_sdf = true;

    const char* cursor = text;
    uint32_t c = 0;

    uint32_t num_added = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        if (dmGameSystem::ResFontHasGlyph(info->m_FontResource, c))
        {
            //printf("  already existed: '%c'\n", c);
            continue;
        }

        uint32_t glyph_index = dmFontGen::CodePointToGlyphIndex(ttfresource, c);
        if (!glyph_index)
            continue; // No such glyph!
        num_added++;

        uint32_t celldatasize = cache_cell_width * cache_cell_height + 1;
        uint8_t* celldata = 0;

        dmGameSystem::FontGlyph fg;

        // Blit the font glyph image into a cache cell image
        if (is_sdf)
        {
            celldata = dmFontGen::GenerateGlyphSdf(ttfresource, glyph_index, scale, info->m_Padding, info->m_EdgeValue, &fg);
        }

        if (!celldata)
        {
            bool is_space = c == ' ';
            if (!is_space)
                continue; // Something went wrong

            celldatasize = 0;
            fg.m_Width = 0;
            fg.m_Height = 0;
            fg.m_Ascent = 0;
            fg.m_Descent = 0;

            // TODO: Get advance for space!
        }

        // The font system takes ownership of the image data
        dmResource::Result r = dmGameSystem::ResFontAddGlyph(info->m_FontResource, c, &fg, celldata, celldatasize);
    }

    uint64_t tend = dmTime::GetTime();
    //printf("Added %u glyphs in %.3fs\n", num_added, (tend-tstart)/1000000.0f);
}

static void RemoveGlyphs(FontInfo* info, const char* text)
{
    const char* cursor = text;
    uint32_t c = 0;

    uint32_t num_added = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        dmGameSystem::ResFontRemoveGlyph(info->m_FontResource, c);
    }
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

    delete g_FontExtContext;
    g_FontExtContext = 0;
}


void Update(dmExtension::Params* params)
{
    // TODO: Add worker thread!
    // TODO: Loop over any finished items

    // static int count = 0;
    // if (count++ == 20)
    // {
    //     FontInfo* info = LoadFont(g_FontExtContext, path, data_path);

    //     if (!info)
    //         return;

    //     const char* text = "DEFdef";
    //     AddGlyphs(info, text);

    //     ResFontDebugPrint(info->m_FontResource);
    // }
}

// Scripting

bool LoadFont(const char* fontc_path, const char* ttf_path)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(dmHashString64(fontc_path));
    if (pinfo)
    {
        dmLogError("Font already loaded %s / %s", fontc_path, ttf_path);
        return false; // Already loaded
    }

    FontInfo* info = LoadFont(ctx, fontc_path, ttf_path);
    return info != 0;
}

bool UnloadFont(dmhash_t fontc_path_hash)
{
    Context* ctx = g_FontExtContext;
    return UnloadFont(ctx, fontc_path_hash);
}


bool AddGlyphs(dmhash_t fontc_path_hash, const char* text)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!pinfo)
    {
        dmLogError("Font not loaded %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    AddGlyphs(*pinfo, text);

    //dmGameSystem::ResFontDebugPrint((*pinfo)->m_FontResource);
    return true;
}


bool RemoveGlyphs(dmhash_t fontc_path_hash, const char* text)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!pinfo)
    {
        dmLogError("Font not loaded %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    RemoveGlyphs(*pinfo, text);

    //dmGameSystem::ResFontDebugPrint((*pinfo)->m_FontResource);
    return true;
}



} // namespace
