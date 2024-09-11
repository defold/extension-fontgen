#pragma once

#include "stb_truetype.h"

namespace dmFontGen {

struct Font
{
    stbtt_fontinfo  m_Font;
    const char*     m_Path;
    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
};

}
