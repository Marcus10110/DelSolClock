#pragma once
#include <Adafruit_ST7789.h>

class GFXcanvas16;

namespace Tft
{
    class Tft
    {
      public:
        Tft();
        void Init();
        void EnableSleep( bool sleep );
        void DrawCanvas( GFXcanvas16* canvas );
        // draw an image from flash without an intermediate buffer.
        void DrawBMPDDirect( char* filename, int16_t x = 0, int16_t y = 0 );

      private:
        Adafruit_ST7789 mTft;
    };
}