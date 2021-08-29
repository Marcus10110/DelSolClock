#include "Display.h"

#include "AppleMediaService.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>

#define TFT_CS 14
#define TFT_RST 15
#define TFT_DC 32

constexpr int16_t TopPadding = 35;
constexpr int16_t BottomPadding = 35;
constexpr int16_t LeftPadding = 0;
constexpr int16_t RightPadding = 0;
constexpr uint16_t DefaultTextColor = ST77XX_WHITE;


Adafruit_ST7735 gDisplay = Adafruit_ST7735( TFT_CS, TFT_DC, TFT_RST );


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

// this is a multiple on the 6x8 character size. 2 = 12x16, etc.
constexpr uint8_t TimeFontSize = 4;
constexpr uint8_t NormalFontSize = 1;

namespace Display
{
    namespace
    {
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
        void GetTextLocation( const char* text, HorizontalAlignment horizontal, VerticalAlignment vertical, int16_t* left, int16_t* top )
        {
            assert( left != nullptr );
            assert( top != nullptr );
            int16_t x1, y1;
            uint16_t w, h;
            gDisplay.getTextBounds( text, 0, 0, &x1, &y1, &w, &h );

            // avoid integer promotion to unsigned.
            auto screen_width = gDisplay.width() - LeftPadding - RightPadding;
            auto screen_height = gDisplay.height() - TopPadding - BottomPadding;

            switch( horizontal )
            {
            case HorizontalAlignment::Left:
                *left = -x1;
                break;
            case HorizontalAlignment::Center:
                *left = ( screen_width - ( int16_t )w ) / 2 - x1;
                break;
            case HorizontalAlignment::Right:
                *left = screen_width - w - x1;
                break;
            }
            *left += LeftPadding;

            switch( vertical )
            {
            case VerticalAlignment::Top:
                *top = -y1;
                break;
            case VerticalAlignment::Center:
                *top = ( screen_height - ( int16_t )h ) / 2 - y1;
                break;
            case VerticalAlignment::Bottom:
                *top = screen_height - h - y1;
                break;
            }
            *top += TopPadding;
        }
    }
    void Begin()
    {
        gDisplay.initR( INITR_BLACKTAB );
        gDisplay.setRotation( 3 );

        gDisplay.fillScreen( ST77XX_BLACK );

        delay( 500 );
        gDisplay.setCursor( 0, 0 );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextWrap( true );
        gDisplay.print( "Booting" );
        Serial.println( "display initialized" );
        delay( 1000 );
        Clear();
        DrawTime( 12, 59 );
        AppleMediaService::MediaInformation info;
        info.mTitle = "Clocks";
        info.mAlbum = "A Rush of Blood to the Head";
        info.mArtist = "Coldplay";
        DrawMediaInfo( info );
        DrawSpeed( 42.314159 );
    }

    void Clear()
    {
        gDisplay.fillScreen( ST77XX_BLACK );
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

        gDisplay.setTextSize( TimeFontSize );
        gDisplay.setTextColor( DefaultTextColor );
        int16_t x, y;
        GetTextLocation( display_string, HorizontalAlignment::Center, VerticalAlignment::Center, &x, &y );


        gDisplay.setCursor( x, y );
        gDisplay.print( display_string );
    }

    void DrawSpeed( float speed )
    {
        if( speed > 1 )
        {
            char display_string[ 20 ] = { 0 };
            snprintf( display_string, sizeof( display_string ), "%.1f mi/h", speed );

            gDisplay.setTextSize( NormalFontSize );
            gDisplay.setTextColor( DefaultTextColor );
            int16_t x, y;
            GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Top, &x, &y );


            gDisplay.setCursor( x, y );
            gDisplay.print( display_string );
        }
    }

    void DrawLightsIndicator( bool on )
    {
    }

    void DrawMediaInfo( const AppleMediaService::MediaInformation& media_info )
    {
        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "%s\n%s - %s", media_info.mTitle.c_str(), media_info.mArtist.c_str(),
                  media_info.mAlbum.c_str() );

        gDisplay.setTextWrap( false );
        gDisplay.setTextSize( NormalFontSize );
        gDisplay.setTextColor( DefaultTextColor );
        int16_t x, y;
        GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );


        gDisplay.setCursor( x, y );
        gDisplay.print( display_string );
    }
}