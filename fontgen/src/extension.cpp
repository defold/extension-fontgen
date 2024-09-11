#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/utf8.h>

#include <dmsdk/gamesys/resources/res_font.h>

//#include "stb_truetype.h"

#include "font.h"
#include "font_render.h"
#include "fontgen.h"

#define MODULE_NAME "fontgen"

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

// static int Reverse(lua_State* L)
// {
//     // The number of expected items to be on the Lua stack
//     // once this struct goes out of scope
//     DM_LUA_STACK_CHECK(L, 1);

//     // Check and get parameter string from stack
//     char* str = (char*)luaL_checkstring(L, 1);

//     // Reverse the string
//     int len = strlen(str);
//     for(int i = 0; i < len / 2; i++) {
//         const char a = str[i];
//         const char b = str[len - i - 1];
//         str[i] = b;
//         str[len - i - 1] = a;
//     }

//     // Put the reverse string on the stack
//     lua_pushstring(L, str);

//     // Return 1 item
//     return 1;
// }

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    //{"reverse", Reverse},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, "fontext", Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeFontGen(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeFontGen(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);

    dmFontGen::Initialize(params);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeFontGen(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeFontGen(dmExtension::Params* params)
{
    dmFontGen::Finalize(params);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateFontGen(dmExtension::Params* params)
{
    dmFontGen::Update(params);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FontGen, MODULE_NAME, AppInitializeFontGen, AppFinalizeFontGen, InitializeFontGen, OnUpdateFontGen, 0, FinalizeFontGen)
