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

      private:
        Adafruit_ST7789 mTft;
    };
}