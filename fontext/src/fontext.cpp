#include <dmsdk/sdk.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// TODO: Use a context for this


stbtt_fontinfo      g_Font;
HResourceFactory    g_ResourceFactory;
void*               g_FontResource = 0;
uint32_t            g_FontResourceSize = 0;


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

static dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams* params)
{
    //dmLogInfo("AppInitializeMyExtension");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeMyExtension(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    //dmLogInfo("Registered %s Extension", MODULE_NAME);

    g_ResourceFactory = params->m_ResourceFactory;

    const char* path = "/assets/fonts/vera_mo_bd.ttf";
    //ResourceResult r = ResourceGetRaw(g_ResourceFactory, path, &g_FontResource, &g_FontResourceSize);
    dmResource::Result r = dmResource::GetRaw(g_ResourceFactory, path, &g_FontResource, &g_FontResourceSize);

    if (dmResource::RESULT_OK == r)
    {
        dmLogWarning("Font load success");

        stbtt_InitFont(&g_Font, (const unsigned char*)g_FontResource, stbtt_GetFontOffsetForIndex((const unsigned char*)g_FontResource,0));
    }
    else
    {
        dmLogWarning("Failed to load font '%s'", path);
    }

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams* params)
{
    //dmLogInfo("AppFinalizeMyExtension");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeMyExtension(dmExtension::Params* params)
{
    //dmLogInfo("FinalizeMyExtension");
    if (g_FontResource)
        free(g_FontResource);
    g_FontResource = 0;
    //     dmResource::Release(g_ResourceFactory, g_FontResource);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateMyExtension(dmExtension::Params* params)
{
    //dmLogInfo("OnUpdateMyExtension");
    return dmExtension::RESULT_OK;
}

static void OnEventMyExtension(dmExtension::Params* params, const dmExtension::Event* event)
{
    switch(event->m_Event)
    {
        case EXTENSION_EVENT_ID_ACTIVATEAPP:
            dmLogInfo("OnEventMyExtension - EVENT_ID_ACTIVATEAPP");
            break;
        case EXTENSION_EVENT_ID_DEACTIVATEAPP:
            dmLogInfo("OnEventMyExtension - EVENT_ID_DEACTIVATEAPP");
            break;
        case EXTENSION_EVENT_ID_ICONIFYAPP:
            dmLogInfo("OnEventMyExtension - EVENT_ID_ICONIFYAPP");
            break;
        case EXTENSION_EVENT_ID_DEICONIFYAPP:
            dmLogInfo("OnEventMyExtension - EVENT_ID_DEICONIFYAPP");
            break;
        default:
            dmLogWarning("OnEventMyExtension - Unknown event id");
            break;
    }
}

DM_DECLARE_EXTENSION(FontExt, "fontext", AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, OnUpdateMyExtension, OnEventMyExtension, FinalizeMyExtension)
