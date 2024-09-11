#pragma once

#include <dmsdk/resource/resource.h>

namespace dmFontGen
{
    bool Initialize(dmExtension::Params* params);
    void Finalize(dmExtension::Params* params);
    void Update(dmExtension::Params* params);
}
