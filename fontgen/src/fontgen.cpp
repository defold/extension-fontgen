#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/utf8.h>

#include <dmsdk/gamesys/resources/res_font.h>

#include "res_ttf.h"
#include "fontgen.h"
#include "job_thread.h"

namespace dmFontGen
{

struct FontInfo
{
    dmGameSystem::FontResource* m_FontResource;
    dmFontGen::TTFResource*     m_TTFResource;
    int                         m_Padding;
    int                         m_EdgeValue;
    float                       m_Scale;

    uint32_t                    m_CacheCellWidth;
    uint32_t                    m_CacheCellHeight;
    uint32_t                    m_CacheCellMaxAscent;
    bool                        m_IsSdf;
};

struct Context
{
    HResourceFactory            m_ResourceFactory;
    dmHashTable64<FontInfo*>    m_FontInfos; // Loaded .fontc files
    dmJobThread::HContext       m_Jobs;
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
        DeleteFont(ctx, info);
        return 0;
    }

    if (dmRenderDDF::TYPE_DISTANCE_FIELD != font_info.m_OutputFormat)
    {
        dmLogError("Currently only distance field fonts are supported: %s", fontc_path);
        DeleteFont(ctx, info);
        return 0;
    }

    info->m_Padding = ctx->m_DefaultSdfPadding;
    if (dmRenderDDF::MODE_MULTI_LAYER == font_info.m_RenderMode)
    {
        info->m_Padding += font_info.m_ShadowBlur + font_info.m_OutlineWidth; // 3 is arbitrary but resembles the output from out generator
    }

    info->m_EdgeValue    = ctx->m_DefaultSdfEdge;
    info->m_Scale        = dmFontGen::SizeToScale(info->m_TTFResource, font_info.m_Size);

    // TODO: Support bitmap fonts
    info->m_IsSdf        = true;

    uint32_t cell_width = 0;
    uint32_t cell_height = 0;
    uint32_t max_ascent = 0;
    dmFontGen::GetCellSize(info->m_TTFResource, &cell_width, &cell_height, &max_ascent);

    info->m_CacheCellWidth = cell_width*info->m_Scale;
    info->m_CacheCellHeight = cell_height*info->m_Scale;
    info->m_CacheCellMaxAscent = info->m_Padding + max_ascent*info->m_Scale;
    r = ResFontSetCacheCellSize(info->m_FontResource, info->m_CacheCellWidth, info->m_CacheCellHeight, info->m_CacheCellMaxAscent);

    if (ctx->m_FontInfos.Full())
    {
        uint32_t cap = ctx->m_FontInfos.Capacity() + 8;
        ctx->m_FontInfos.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_FontInfos.Put(path_hash, info);

    dmLogInfo("Loaded font '%s", fontc_path);
    return info;
}

// Only called at shutdown of the extension
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

// ****************************************************************************************************

struct JobItem
{
    // input
    FontInfo* m_FontInfo;
    uint32_t  m_Codepoint;
    // output
    dmGameSystem::FontGlyph m_Glyph;
    uint8_t*                m_Data;     // May be 0. First byte is the compression (0=no compression, 1=deflate)
    uint32_t                m_DataSize;
};

// Called on the worker thread
static int JobGenerateGlyph(void* context, void* data)
{
    Context* ctx = (Context*)context;
    JobItem* item = (JobItem*)data;
    FontInfo* info = item->m_FontInfo;
    uint32_t codepoint = item->m_Codepoint;

    //
    TTFResource* ttfresource = info->m_TTFResource;
    uint32_t glyph_index = dmFontGen::CodePointToGlyphIndex(ttfresource, codepoint);
    if (!glyph_index)
        return 0;

    item->m_Data = 0;
    item->m_DataSize = 1 + info->m_CacheCellWidth * info->m_CacheCellHeight;
    if (info->m_IsSdf)
    {
        item->m_Data = dmFontGen::GenerateGlyphSdf(ttfresource, glyph_index, info->m_Scale, info->m_Padding, info->m_EdgeValue, &item->m_Glyph);
    }

    if (!item->m_Data) // Some glyphs (e.g. ' ') don't have an image, which is ok
    {
        bool is_space = codepoint == ' ';
        if (!is_space)
            return 0; // Something went wrong

        item->m_DataSize = 0;
        item->m_Glyph.m_Width = 0;
        item->m_Glyph.m_Height = 0;
        item->m_Glyph.m_Ascent = 0;
        item->m_Glyph.m_Descent = 0;
    }
    return 1;
}

// Called on the main thread
static void JobPostProcessGlyph(void* context, void* data, int result)
{
    Context* ctx = (Context*)context;
    JobItem* item = (JobItem*)data;
    uint32_t codepoint = item->m_Codepoint;

    if (!result)
    {
        dmLogWarning("Failed to generate glyph '%c'", codepoint);
        delete item;
        return;
    }

    // The font system takes ownership of the image data
    dmResource::Result r = dmGameSystem::ResFontAddGlyph(item->m_FontInfo->m_FontResource, codepoint, &item->m_Glyph, item->m_Data, item->m_DataSize);
    delete item;
}

// ****************************************************************************************************

static void GenerateGlyph(Context* ctx, FontInfo* info, uint32_t codepoint)
{
    JobItem* item = new JobItem;
    item->m_FontInfo = info;
    item->m_Codepoint = codepoint;
    dmJobThread::PushJob(ctx->m_Jobs, JobGenerateGlyph, JobPostProcessGlyph, ctx, item);
}

static void GenerateGlyphs(Context* ctx, FontInfo* info, const char* text)
{
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

        GenerateGlyph(ctx, info, c);
    }
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

    dmJobThread::JobThreadCreationParams job_thread_create_param;
    job_thread_create_param.m_ThreadNames[0] = "FontGenJobThread";
    job_thread_create_param.m_ThreadCount    = 1;
    g_FontExtContext->m_Jobs = dmJobThread::Create(job_thread_create_param);
    return true;
}

void Finalize(dmExtension::Params* params)
{
    if (g_FontExtContext->m_Jobs)
        dmJobThread::Destroy(g_FontExtContext->m_Jobs);

    g_FontExtContext->m_FontInfos.Iterate(DeleteFontInfoIter, g_FontExtContext);
    g_FontExtContext->m_FontInfos.Clear();

    delete g_FontExtContext;
    g_FontExtContext = 0;
}


void Update(dmExtension::Params* params)
{
    if (g_FontExtContext->m_Jobs)
        dmJobThread::Update(g_FontExtContext->m_Jobs, 1000); // Update for max 1 millisecond on non-threaded systems
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

    GenerateGlyphs(ctx, *pinfo, text);

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
