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


namespace Display
{
    namespace
    {
        SPIClass* gDisplaySpi = BuildDisplaySpi();
        Adafruit_ST7789 gTft = Adafruit_ST7789( gDisplaySpi, Pin::TftCs, Pin::TftDc, Pin::TftReset );
        GFXcanvas16 gDisplayBuffer = GFXcanvas16( 240, 136 );
        SPIFFS_ImageReader reader;
        DisplayState State;
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

#define DRAW_WITH_BUFFER
#ifdef DRAW_WITH_BUFFER
        auto& gDisplay = gDisplayBuffer;
        void DisplayCanvas()
        {
            gTft.drawRGBBitmap( 0, 0, gDisplayBuffer.getBuffer(), gDisplayBuffer.width(), gDisplayBuffer.height() );
        }
#else
        auto& gDisplay = gTft;
        void DisplayCanvas()
        {
        }
#endif


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

    bool DisplayState::operator==( const DisplayState& rhs ) const
    {
        return mHours24 == rhs.mHours24 && mMinutes == rhs.mMinutes && mSpeed == rhs.mSpeed && mIconHeadlight == rhs.mIconHeadlight &&
               mIconBluetooth == rhs.mIconBluetooth && mIconShuffle == rhs.mIconShuffle && mIconRepeat == rhs.mIconRepeat &&
               mMediaTitle == rhs.mMediaTitle && mMediaArtist == rhs.mMediaArtist;
    }

    bool DisplayState::operator!=( const DisplayState& rhs ) const
    {
        return !( *this == rhs );
    }

    void DisplayState::Dump() const
    {
        Serial.printf( "time: %u:%u\n", mHours24, mMinutes );
        Serial.printf( "speed: %f\n", mSpeed );
        Serial.printf( "icons: %i, %i, %i, %i\n", mIconHeadlight, mIconBluetooth, mIconShuffle, mIconRepeat );
        Serial.printf( "title: %s\n", mMediaTitle.c_str() );
        Serial.printf( "artist: %s\n", mMediaArtist.c_str() );
    }

    void Begin()
    {
        gTft.init( 136, 240 ); // one extra pixel removes that ugly garbage line at the bottom of the display.
        Serial.println( "display initialized" );
        gTft.setRotation( 3 );

        ScreenRect.x = LeftPadding;
        ScreenRect.y = TopPadding;
        ScreenRect.w = gDisplay.width() - LeftPadding - RightPadding;
        ScreenRect.h = gDisplay.height() - TopPadding - BottomPadding;

        Clear();
        DisplayCanvas();
        gDisplay.setCursor( 0, 0 );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextWrap( true );
        gDisplay.setFont( NormalFont );
        gDisplay.setTextSize( NormalFontSize );

        if( !SPIFFS.begin() )
        {
            Serial.println( "SPIFFS failed" );
            return;
        }
    }

    void WriteDisplay()
    {
        DisplayCanvas();
    }

    void DrawBMP( char* path, int16_t x, int16_t y )
    {
#ifdef DRAW_WITH_BUFFER
        SPIFFS_Image image;
        reader.loadBMP( path, image );
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
        gDisplayBuffer.drawRGBBitmap( x, y, image_canvas->getBuffer(), image_canvas->width(), image_canvas->height() );
#else
        reader.drawBMP( path, gTft, x, y );
#endif
    }

    void DrawSplash()
    {
        Clear();
        DrawBMP( "/OldSols.bmp", 0, 0 );
    }

    void DrawLightAlarm()
    {
        DrawBMP( "/light_large.bmp", 68, 16 ); // 104x104
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextSize( 2 );
        int16_t x, y;
        const char* str = "LIGHTS ON";
        GetTextLocation( str, HorizontalAlignment::Center, VerticalAlignment::Bottom, &x, &y );
        gDisplay.setCursor( x, y );
        gDisplay.print( str );
        gDisplay.setTextSize( NormalFontSize );
    }

    void Clear()
    {
        gDisplay.fillScreen( DefaultBackgroundColor );
        // reset the state.
        State = DisplayState{};
    }

    void DrawTime( uint8_t hours24, uint8_t minutes )
    {
        State.mHours24 = hours24;
        State.mMinutes = minutes;
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
        State.mSpeed = speed;
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
            State.mIconHeadlight = visible;
            break;
        case Icon::Bluetooth:
            filename = "/bluetooth.bmp"; // 14x22
            y = top;
            State.mIconBluetooth = visible;
            if( !visible )
            {
                // Hack: display red logo instead
                visible = true;
                filename = "/bluetooth_red.bmp"; // 14x22
            }
            break;
        case Icon::Shuffle:
            filename = "/shuffle.bmp"; // 24x24
            y = top + 24;
            State.mIconShuffle = visible;
            break;
        case Icon::Repeat:
            filename = "/repeat.bmp"; // 24x24
            y = top + 24 * 2;
            State.mIconRepeat = visible;
            break;
        default:
            break;
        }

        if( visible )
        {
            DrawBMP( filename, x, y );
        }
    }

    void DrawMediaInfo( const std::string& title, const std::string& artist )
    {
        State.mMediaArtist = artist;
        State.mMediaTitle = title;
        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "%s\n%s", title.c_str(), artist.c_str() );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.setTextWrap( false );
        int16_t x, y;
        GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );
        gDisplay.setCursor( x, y );
        gDisplay.print( display_string );
        gDisplay.setTextWrap( true );
    }

    void DrawDebugInfo( const std::string& message, bool center, bool fine_print )
    {
        if( fine_print )
        {
            gDisplay.setFont( nullptr );
        }
        gDisplay.setTextColor( DefaultTextColor );
        int16_t x, y;
        auto str = message.c_str();
        if( center )
        {
            GetTextLocation( str, HorizontalAlignment::Center, VerticalAlignment::Center, &x, &y );
        }
        else
        {
            GetTextLocation( str, HorizontalAlignment::Center, VerticalAlignment::Bottom, &x, &y );
        }

        gDisplay.setCursor( x, y );
        gDisplay.print( str );
        gDisplay.setFont( NormalFont );
    }

    void EnableSleep( bool sleep )
    {
        gTft.enableSleep( sleep );
        CarIO::SetTftBrightness( sleep ? 0 : 255 );
    }

    const DisplayState& GetState()
    {
        return State;
    }

    void DrawState( const DisplayState& state )
    {
        Clear();
        DrawTime( state.mHours24, state.mMinutes );
        DrawSpeed( state.mSpeed );
        DrawIcon( Icon::Headlight, state.mIconHeadlight );
        DrawIcon( Icon::Bluetooth, state.mIconBluetooth );
        DrawIcon( Icon::Shuffle, state.mIconShuffle );
        DrawIcon( Icon::Repeat, state.mIconRepeat );
        if( !state.mMediaTitle.empty() || !state.mMediaArtist.empty() )
        {
            DrawMediaInfo( state.mMediaTitle, state.mMediaArtist );
        }
        else
        {
            State.mMediaArtist = "";
            State.mMediaTitle = "";
        }
    }
}