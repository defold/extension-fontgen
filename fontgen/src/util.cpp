#include "util.h"
#include <stdio.h>

namespace dmFont {

void DebugPrintBitmap(uint8_t* bitmap, int w, int h)
{
    printf("--------------------------------------------\n");
    for (int j=0; j < h; ++j)
    {
        for (int i=0; i < w; ++i)
        {
            putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
        }
        putchar('\n');
    }
    printf("--------------------------------------------\n");
}

} // namespace
