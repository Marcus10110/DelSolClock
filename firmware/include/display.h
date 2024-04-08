#pragma once

#include <Adafruit_GFX.h>

#include "fonts/digital_7__mono_40pt7b.h"
#include "Fonts/FreeMono9pt7b.h"
#include "Fonts/FreeMono12pt7b.h"

namespace Display
{
    constexpr int16_t TopPadding = 5;
    constexpr int16_t BottomPadding = 10;
    constexpr int16_t LeftPadding = 0;
    constexpr int16_t RightPadding = 0;
    constexpr int16_t IconPlaceholderWidth = 70;
    constexpr uint16_t DefaultTextColor = 0xFFFF;
    constexpr uint16_t DefaultBackgroundColor = 0x0000;

    constexpr uint8_t TimeFontSize = 1;
    constexpr GFXfont const* TimeFont = &digital_7__mono_40pt7b;
    constexpr uint8_t NormalFontSize = 1;
    constexpr GFXfont const* NormalFont = &FreeMono9pt7b;
    constexpr uint8_t SpeedFontSize = 2;
    constexpr uint8_t MusicFontSize = 2;

    enum class HorizontalAlignment
    {
        Left,
        Center,
        Right
    };
    enum class VerticalAlignment
    {
        Top,
        Center,
        Bottom
    };

    struct Rect
    {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
    };

    class Display : public GFXcanvas16
    {
        typedef void ( *ImageLoaderFn )( GFXcanvas16* destination, char* path, int16_t x, int16_t y );

      public:
        Display( ImageLoaderFn image_loader = nullptr );
        void clear();
        void DrawBMP( char* path, int16_t x, int16_t y );
        void GetTextLocation( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, int16_t* left, int16_t* top,
                              Rect* within_region = nullptr, Rect* text_region = nullptr );
        void WriteAligned( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, Rect* within_region = nullptr,
                           Rect* text_region = nullptr );
        const Rect& ScreenRect();

      private:
        Rect mScreenRect;
        ImageLoaderFn mImageLoader;
    };
}