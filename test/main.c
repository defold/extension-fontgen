#include <stdio.h>
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

unsigned char ttf_buffer[1<<25];

static void printBitmap(unsigned char* bitmap, int w, int h)
{
   int i,j;
   for (j=0; j < h; ++j) {
      for (i=0; i < w; ++i)
         putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
      putchar('\n');
   }
}

int main(int argc, char **argv)
{
   stbtt_fontinfo font;
   unsigned char *bitmap;

   int c = (argc > 1 ? argv[1][0] : 'a');
   int s = (argc > 2 ? atoi(argv[2]) : 20);
   const char* path = argc > 3 ? argv[3] : "c:/windows/fonts/arialbd.ttf";

   printf("Reading char '%c' %d at size %d  from font %s\n", c, c, s, path);

   fread(ttf_buffer, 1, 1<<25, fopen(path, "rb"));

   stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));

   float scale = stbtt_ScaleForPixelHeight(&font, s);

   int ascent, descent, linegap;
   stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);

   printf("font: asc/desc: %.2f, %.2f  linegap: %.2f\n", ascent*scale, descent*scale, linegap*scale);

   int x0, x1, y0, y1;
   stbtt_GetFontBoundingBox(&font, &x0, &y0, &x1, &y1);
   float bx0 = x0 * scale;
   float bx1 = x1 * scale;
   float by0 = y0 * scale;
   float by1 = y1 * scale;
   printf("    bbox: %.2f, %.2f   %.2f, %.2f  (%.2f, %.2f)\n", bx0, by0, bx1, by1, (bx1-bx0), (by1-by0));


   int w, h;
   int xoff, yoff;
   bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &w, &h, &xoff, &yoff);

   int g = stbtt_FindGlyphIndex(&font, c);

   printf("g: %d\n", g);

   int advx, advy;
   stbtt_GetGlyphHMetrics(&font, g, &advx, &advy);

   printf("glyph: w/h: %d, %d off: %d, %d  adv: %.2f, %.2f\n", w, h, xoff, yoff, advx*scale, advy*scale);

   printBitmap(bitmap, w, h);
   stbtt_FreeBitmap(bitmap, 0);
   printf("\n");

   int padding = 3;
   unsigned char edge_value = 180;
   float pixel_dist_scale = (float)edge_value/(float)padding;
   bitmap = stbtt_GetGlyphSDF(&font, scale, g, padding, edge_value, pixel_dist_scale, &w, &h, &xoff, &yoff);

   printf("glyph: w/h: %d, %d off: %d, %d  adv: %.2f, %.2f\n", w, h, xoff, yoff, advx*scale, advy*scale);

   printBitmap(bitmap, w, h);
   stbtt_FreeBitmap(bitmap, 0);
   printf("\n");

   return 0;
}
