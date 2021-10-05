#include "Display.h"

#include "AppleMediaService.h"
#include "pins.h"
#include "CarIO.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <SPIFFS_ImageReader.h>
#include "digital_7__mono_40pt7b.h"

#include "Fonts/FreeMono9pt7b.h"
#include "Fonts/FreeMono12pt7b.h"

constexpr int16_t TopPadding = 0;
constexpr int16_t BottomPadding = 0;
constexpr int16_t LeftPadding = 0;
constexpr int16_t RightPadding = 0;
constexpr int16_t IconPlaceholderWidth = 70;
constexpr uint16_t DefaultTextColor = ST77XX_WHITE;
constexpr uint16_t DefaultBackgroundColor = ST77XX_BLACK;

constexpr uint8_t TimeFontSize = 1;
constexpr GFXfont const* TimeFont = &digital_7__mono_40pt7b;
constexpr uint8_t NormalFontSize = 1;
constexpr GFXfont const* NormalFont = &FreeMono9pt7b;


/*
Ideas for display content:
Time
full date

Music:
track name
loop icon
repeat icon

Bluetooth:
not connected / connecting / connected
*/

SPIClass* BuildDisplaySpi()
{
    SPIClass* vspi = new SPIClass( VSPI );
    vspi->begin( Pin::Sck, Pin::Miso, Pin::Mosi, Pin::TftCs );
    // vspi->end();
    return vspi;
}

SPIClass* gDisplaySpi = BuildDisplaySpi();
Adafruit_ST7789 gDisplay = Adafruit_ST7789( gDisplaySpi, Pin::TftCs, Pin::TftDc, Pin::TftReset );
SPIFFS_ImageReader reader;

namespace Display
{
    namespace
    {
        struct Rect
        {
            int16_t x;
            int16_t y;
            int16_t w;
            int16_t h;
        };

        Rect ScreenRect;

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
        // get text position based on some settings. Uses current font settings, etc.
        // Note: this can't handle wrapping text for anything narrower than the screen width. For that we would need to re-implement
        // getTextBounds
        void GetTextLocation( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, int16_t* left, int16_t* top,
                              Rect* within_region = nullptr, Rect* text_region = nullptr )
        {
            assert( left != nullptr );
            assert( top != nullptr );
            if( !within_region )
            {
                within_region = &ScreenRect;
            }

            int16_t x1, y1;
            uint16_t w, h;
            gDisplay.getTextBounds( text, 0, 0, &x1, &y1, &w, &h );

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
    }
    void Begin()
    {
        gDisplay.init( 135, 240 );
        Serial.println( "display initialized" );
        gDisplay.setRotation( 3 );

        ScreenRect.x = LeftPadding;
        ScreenRect.y = TopPadding;
        ScreenRect.w = gDisplay.width() - LeftPadding - RightPadding;
        ScreenRect.h = gDisplay.height() - TopPadding - BottomPadding;

        Clear();
        gDisplay.setCursor( 0, 0 );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextWrap( true );
        gDisplay.setFont( NormalFont );
        gDisplay.setTextSize( NormalFontSize );
        /*
        // font test.
        gDisplay.setTextSize( 1 );
        gDisplay.setFont( nullptr );
        gDisplay.println( "Default font, 1x size displayed" );
        gDisplay.setTextSize( 2 );
        gDisplay.println( "Default Font, 2x size displayed" );
        gDisplay.setTextSize( 1 );
        gDisplay.setFont( &FreeMono9pt7b );
        gDisplay.println( "FreeMono9pt7b Font, 1x size displayed" );
        gDisplay.setFont( &FreeMono12pt7b );
        gDisplay.println( "FreeMono12pt7b Font, 1x size displayed" );

        delay( 60000 );
        */

        /*
        DrawTime( 12, 59 );
        AppleMediaService::MediaInformation info;
        info.mTitle = "Clocks";
        info.mAlbum = "A Rush of Blood to the Head";
        info.mArtist = "Coldplay";
        DrawMediaInfo( info );
        DrawSpeed( 42.314159 );
        */

        if( !SPIFFS.begin() )
        {
            Serial.println( "SPIFFS failed" );
            return;
        }
        reader.drawBMP( "/OldSols.bmp", gDisplay, 0, 0 );
        delay( 5000 );
    }

    void Clear()
    {
        gDisplay.fillScreen( DefaultBackgroundColor );
    }

    void DrawTime( uint8_t hours24, uint8_t minutes )
    {
        bool isAfternoon = hours24 >= 12;
        uint8_t hours12 = 0;
        if( hours24 == 0 )
        {
            hours12 = 12;
        }
        else if( hours24 > 12 )
        {
            hours12 = hours24 - 12;
        }
        else
        {
            hours12 = hours24;
        }
        char display_string[ 10 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "%i:%02i", hours12, minutes );
        gDisplay.setFont( TimeFont );
        gDisplay.setTextSize( TimeFontSize );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextWrap( false ); // disable wrap just for this function.
        int16_t x, y;
        Rect time_region;
        GetTextLocation( display_string, HorizontalAlignment::Center, VerticalAlignment::Center, &x, &y, nullptr, &time_region );
        gDisplay.setCursor( x, y );
        gDisplay.print( display_string );

        // Draw AM / PM indicator
        gDisplay.setFont( NormalFont );
        gDisplay.setTextSize( NormalFontSize );
        gDisplay.setTextWrap( false );
        Rect afternoon_region;
        afternoon_region.x = time_region.x + time_region.w + 8;
        afternoon_region.y = time_region.y;
        afternoon_region.h = time_region.h;
        afternoon_region.w = ScreenRect.x + ScreenRect.w - afternoon_region.x - afternoon_region.w;
        GetTextLocation( isAfternoon ? "PM" : "AM", HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y, &afternoon_region );
        gDisplay.setCursor( x, y );
        gDisplay.print( isAfternoon ? "PM" : "AM" );
        gDisplay.setTextWrap( true ); // turn wrap back on
    }

    void DrawSpeed( float speed )
    {
        if( speed > 1 )
        {
            char display_string[ 20 ] = { 0 };
            snprintf( display_string, sizeof( display_string ), "%.1f mi/h", speed );

            gDisplay.setTextColor( DefaultTextColor );
            int16_t x, y;
            GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Top, &x, &y );


            gDisplay.setCursor( x, y );
            gDisplay.print( display_string );
        }
    }

    void DrawIcon( Icon icon, bool visible )
    {
        char* filename;
        int16_t x = 0;
        int16_t y = 0;
        int16_t top = ( ScreenRect.h - 24 * 3 ) / 2 + ScreenRect.y;

        switch( icon )
        {
        case Icon::Headlight:
            filename = "/light_small.bmp"; // 32x32
            y = 0;
            x = ScreenRect.x + ScreenRect.w - 32;
            break;
        case Icon::Bluetooth:
            filename = "/bluetooth.bmp"; // 14x22
            y = top;
            break;
        case Icon::Shuffle:
            filename = "/shuffle.bmp"; // 24x24
            y = top + 24;
            break;
        case Icon::Repeat:
            filename = "/repeat.bmp"; // 24x24
            y = top + 24 * 2;
            break;
        default:
            break;
        }

        if( visible )
        {
            Serial.print( "icon x: " );
            Serial.print( x );
            Serial.print( " y: " );
            Serial.print( y );
            Serial.print( " file: " );
            Serial.println( filename );
            reader.drawBMP( filename, gDisplay, x, y );
        }
    }

    void DrawMediaInfo( const AppleMediaService::MediaInformation& media_info )
    {
        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "%s\n%s", media_info.mTitle.c_str(), media_info.mArtist.c_str() );
        gDisplay.setTextColor( DefaultTextColor );
        int16_t x, y;
        GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );
        gDisplay.setCursor( x, y );
        gDisplay.print( display_string );
    }

    void DrawDebugInfo( const std::string& message )
    {
        gDisplay.setFont( nullptr );
        gDisplay.setTextColor( DefaultTextColor );
        int16_t x, y;
        auto str = message.c_str();
        GetTextLocation( str, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );
        gDisplay.setCursor( x, y );
        gDisplay.print( str );
        gDisplay.setFont( NormalFont );
    }

    void EnableSleep( bool sleep )
    {
        gDisplay.enableSleep( sleep );
        CarIO::SetTftBrightness( sleep ? 0 : 255 );
    }
}