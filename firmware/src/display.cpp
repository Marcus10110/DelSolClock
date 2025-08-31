#include "display.h"
#include <SPIFFS_ImageReader.h>
#include <map>
#include <string>
#include "logger.h"
namespace Display
{
    namespace
    {
        // cache the loaded images, because image load time is SLOW (5 sec for splash)
        std::map<std::string, SPIFFS_Image> mLoadedImages;


        void DefaultImageLoader( GFXcanvas16* destination, char* path, int16_t x, int16_t y, bool delete_after_draw,
                                 uint16_t monochrome_color )
        {
            // LOG_TRACE( "loading/drawing image %s", path );
            if( !PreloadImage( path ) )
            {
                LOG_ERROR( "Failed to preload image %s", path );
                return;
            }

            auto& loaded_image = mLoadedImages.at( path );
            void* raw_canvas = loaded_image.getCanvas();
            if( raw_canvas == nullptr )
            {
                LOG_ERROR( "Failed to get canvas from cached image %s", path );
                return;
            }

            if( loaded_image.getFormat() == IMAGE_16 )
            {
                GFXcanvas16* image_canvas = static_cast<GFXcanvas16*>( raw_canvas );
                destination->drawRGBBitmap( x, y, image_canvas->getBuffer(), image_canvas->width(), image_canvas->height() );
            }
            else if( loaded_image.getFormat() == IMAGE_1 )
            {
                GFXcanvas1* image_canvas = static_cast<GFXcanvas1*>( raw_canvas );
                destination->drawBitmap( x, y, image_canvas->getBuffer(), image_canvas->width(), image_canvas->height(), monochrome_color );
            }
            else
            {
                LOG_ERROR( "Unsupported image format %u", loaded_image.getFormat() );
            }

            if( delete_after_draw )
            {
                mLoadedImages.erase( path );
            }
        }
    }

    bool PreloadImage( char* path )
    {
        if( mLoadedImages.count( path ) == 0 )
        {
            auto start_time = millis();
            LOG_INFO( "%s not already loaded. loading now. \tFree Heap: %d", path, ESP.getFreeHeap() );
            SPIFFS_ImageReader reader;
            SPIFFS_Image& image = mLoadedImages[ path ];
            auto load_result = reader.loadBMP( path, image );
            if( load_result != IMAGE_SUCCESS )
            {
                // failed to load image
                LOG_ERROR( "Failed to load image %s, code: %u", path, load_result );
                if( load_result == IMAGE_ERR_MALLOC )
                {
                    LOG_INFO( "free memory: %d", ESP.getFreeHeap() );
                }
                // remove from the collection
                mLoadedImages.erase( path );
                return false;
            }
            auto format = image.getFormat();
            if( format != IMAGE_16 && format != IMAGE_1 )
            {
                LOG_ERROR( "Unsupported image format %u", format );
                // remove from the collection:
                mLoadedImages.erase( path );
                return false;
            }

            // print buffer size in bytes
            auto duration = millis() - start_time;
            auto size_bytes = format == IMAGE_16 ? image.width() * image.height() * 2 : ( ( image.width() + 7 ) / 8 * image.height() );
            LOG_INFO( "Loaded image %s, \tbuffer size: %d bytes. \tFree heap: %d. \tDuration MS: %d", path, size_bytes, ESP.getFreeHeap(),
                      duration );
        }
        return true;
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
        setFont( nullptr );
        setTextSize( DefaultFontSize );
    }

    void Display::DrawBMP( char* path, int16_t x, int16_t y, bool delete_after_draw, uint16_t monochrome_color )
    {
        assert( mImageLoader != nullptr );
        mImageLoader( this, path, x, y, delete_after_draw, monochrome_color );
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