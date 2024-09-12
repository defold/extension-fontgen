#include <dmsdk/sdk.h>
#include <dmsdk/dlib/hash.h>

#include "fontgen.h"

#define MODULE_NAME "fontgen"


static int LoadFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    const char* fontc_path = luaL_checkstring(L, 1); // dmScript::CheckHash(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2); //dmScript::CheckHash(L, 2);

    if (!dmFontGen::LoadFont(fontc_path, ttf_path))
    {
        lua_pushnil(L); // No font
        lua_pushfstring(L, "Failed to load one of fonts: %s / %s", fontc_path, ttf_path);
    }
    else
    {
        dmScript::PushHash(L, dmHashString64(fontc_path));
        lua_pushnil(L); // no error
    }
    return 2;
}

static int UnloadFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    if (!dmFontGen::UnloadFont(fontc_path_hash))
        return luaL_error(L, "Failed to unload font %s", dmHashReverseSafe64(fontc_path_hash));

    return 0;
}

static int AddGlyphs(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    const char* text = luaL_checkstring(L, 2);

    if (!dmFontGen::AddGlyphs(fontc_path_hash, text))
        return luaL_error(L, "Failed to add glyphs to font %s", dmHashReverseSafe64(fontc_path_hash));

    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"load_font", LoadFont},
    {"unload_font", UnloadFont},
    {"add_glyphs", AddGlyphs},
    // {"remove_glyphs", RemoveGlyphs},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
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
    dmLogInfo("Registered %s extension", MODULE_NAME);

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
