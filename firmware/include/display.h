#pragma once

#include <Adafruit_GFX.h>


namespace Display
{
    constexpr int16_t TopPadding = 5;
    constexpr int16_t BottomPadding = 10;
    constexpr int16_t LeftPadding = 0;
    constexpr int16_t RightPadding = 0;
    constexpr uint16_t DefaultTextColor = 0xFFFF;
    constexpr uint16_t DefaultBackgroundColor = 0x0000;
    constexpr uint8_t DefaultFontSize = 1;

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
        int16_t x{ 0 };
        int16_t y{ 0 };
        int16_t w{ 0 };
        int16_t h{ 0 };
    };

    class Display : public GFXcanvas16
    {
        typedef void ( *ImageLoaderFn )( GFXcanvas16* destination, char* path, int16_t x, int16_t y, bool delete_after_draw );

      public:
        Display( ImageLoaderFn image_loader = nullptr );
        void clear();
        void DrawBMP( char* path, int16_t x, int16_t y, bool delete_after_draw = false );
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