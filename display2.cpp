#include "display2.h"
#include <SPIFFS_ImageReader.h>

namespace Display
{
    namespace
    {
        void DefaultImageLoader( GFXcanvas16* destination, char* path, int16_t x, int16_t y )
        {
            SPIFFS_ImageReader reader;
            SPIFFS_Image image;
            if( reader.loadBMP( path, image ) != IMAGE_SUCCESS )
            {
                // failed to load image
                return;
            }
            auto format = image.getFormat();
            if( format != IMAGE_16 )
            {
                return;
            }
            auto raw_canvas = image.getCanvas();
            if( raw_canvas == nullptr )

            {
                return;
            }
            GFXcanvas16* image_canvas = static_cast<GFXcanvas16*>( raw_canvas );
            destination->drawRGBBitmap( x, y, image_canvas->getBuffer(), image_canvas->width(), image_canvas->height() );
        }
    }
    Display::Display( ImageLoaderFn image_loader )
        : GFXcanvas16( 240, 136 ), mImageLoader( image_loader ? image_loader : DefaultImageLoader )
    {
        mScreenRect.x = LeftPadding;
        mScreenRect.y = TopPadding;
        mScreenRect.w = width() - LeftPadding - RightPadding;
        mScreenRect.h = height() - TopPadding - BottomPadding;
    }

    void Display::clear()
    {
        fillScreen( DefaultBackgroundColor );
        // restore all the defaults too.
        setCursor( 0, 0 );
        setTextColor( DefaultTextColor );
        setTextWrap( true );
        setFont( NormalFont );
        setTextSize( NormalFontSize );
    }

    void Display::DrawBMP( char* path, int16_t x, int16_t y )
    {
        assert( mImageLoader != nullptr );
        mImageLoader( this, path, x, y );
    }

    // get text position based on some settings. Uses current font settings, etc.
    // Note: this can't handle wrapping text for anything narrower than the screen width. For that we would need to re-implement
    // getTextBounds
    void Display::GetTextLocation( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, int16_t* left,
                                   int16_t* top, Rect* within_region, Rect* text_region )
    {
        assert( left != nullptr );
        assert( top != nullptr );
        if( !within_region )
        {
            within_region = &mScreenRect;
        }

        int16_t x1, y1;
        uint16_t w, h;
        getTextBounds( text, 0, 0, &x1, &y1, &w, &h );

        switch( horizontal )
        {
        case HorizontalAlignment::Left:
            *left = -x1;
            break;
        case HorizontalAlignment::Center:
            *left = ( within_region->w - ( int16_t )w ) / 2 - x1;
            break;
        case HorizontalAlignment::Right:
            *left = within_region->w - w - x1;
            break;
        }
        *left += within_region->x;

        switch( vertical )
        {
        case VerticalAlignment::Top:
            *top = -y1;
            break;
        case VerticalAlignment::Center:
            *top = ( within_region->h - ( int16_t )h ) / 2 - y1;
            break;
        case VerticalAlignment::Bottom:
            *top = within_region->h - h - y1;
            break;
        }
        *top += within_region->y;

        if( text_region )
        {
            text_region->x = *left + x1;
            text_region->y = *top + y1;
            text_region->w = w;
            text_region->h = h;
        }
    }

    void Display::WriteAligned( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, Rect* within_region,
                                Rect* text_region )
    {
        int16_t left, top;
        GetTextLocation( text, horizontal, vertical, &left, &top, within_region, text_region );
        setCursor( left, top );
        write( text );
    }

    const Rect& Display::ScreenRect()
    {
        return mScreenRect;
    }


}