#pragma once

#include <stdint.h>

namespace dmFontGen
{
    /*
     * Outputs a w*h single channel bitmap to stdout
     */
    void DebugPrintBitmap(uint8_t* bitmap, int w, int h);
}
