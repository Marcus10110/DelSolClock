#include "screens.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>

#include <SPI.h>
#include <SPIFFS_ImageReader.h>
#include <string>


using namespace Display;

namespace Screens
{
    void Splash::Draw( Display::Display* display )
    {
        display->clear();
        display->DrawBMP( "/OldSols.bmp", 0, 0 );
    }

    void Clock::Draw( Display::Display* display )
    {
        display->clear();

        bool isAfternoon = mHours24 >= 12;
        uint8_t hours12 = 0;
        if( mHours24 == 0 )
        {
            hours12 = 12;
        }
        else if( mHours24 > 12 )
        {
            hours12 = mHours24 - 12;
        }
        else
        {
            hours12 = mHours24;
        }

        char display_string[ 128 ] = { 0 };
        snprintf( display_string, sizeof( display_string ), "%i:%02i", hours12, mMinutes );
        display->setFont( TimeFont );
        display->setTextSize( TimeFontSize );
        display->setTextColor( DefaultTextColor );
        display->setTextWrap( false ); // disable wrap just for this function.
        int16_t x, y;
        Rect time_region;
        display->GetTextLocation( display_string, HorizontalAlignment::Center, VerticalAlignment::Center, &x, &y, nullptr, &time_region );
        display->setCursor( x, y );
        display->print( display_string );

        // Draw AM / PM indicator
        display->setFont( NormalFont );
        display->setTextSize( NormalFontSize );
        display->setTextWrap( false );
        Rect afternoon_region;
        afternoon_region.x = time_region.x + time_region.w + 8;
        afternoon_region.y = time_region.y;
        afternoon_region.h = time_region.h;
        const auto& screen_rect = display->ScreenRect();
        afternoon_region.w = screen_rect.x + screen_rect.w - afternoon_region.x - afternoon_region.w;
        display->GetTextLocation( isAfternoon ? "PM" : "AM", HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y,
                                  &afternoon_region );
        display->setCursor( x, y );
        display->print( isAfternoon ? "PM" : "AM" );
        display->setTextWrap( true ); // turn wrap back on

        // Music info
        snprintf( display_string, sizeof( display_string ), "%s", mMediaTitle.c_str() );
        // snprintf( display_string, sizeof( display_string ), "%s\n%s", title.c_str(), artist.c_str() );
        display->setTextColor( DefaultTextColor );
        display->setTextSize( MusicFontSize );
        display->setTextWrap( false );
        display->GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );
        display->setCursor( x, y );
        display->print( display_string );
        display->setTextWrap( true );
        display->setTextSize( NormalFontSize );


        // speed
        if( mSpeed > 10 )
        {
            snprintf( display_string, sizeof( display_string ), "%.0f", mSpeed );
            char mph[] = "MPH";
            display->setTextColor( DefaultTextColor );
            display->setTextSize( SpeedFontSize );
            display->WriteAligned( display_string, HorizontalAlignment::Left, VerticalAlignment::Top );
            display->setTextSize( NormalFontSize );

            // add MPH
            display->print( mph );
        }

        // headlight
        if( mHeadlight )
        {
            display->DrawBMP( "/light_small.bmp", screen_rect.x + screen_rect.w - 32, 0 );
        }

        // bluetooth
        int16_t top = ( screen_rect.h - 24 * 3 ) / 2 + screen_rect.y;
        if( mBluetooth )
        {
            display->DrawBMP( "/bluetooth.bmp", 0, top );
        }
        else
        {
            display->DrawBMP( "/bluetooth_red.bmp", 0, top );
        }
    }

    void LightsAlarm::Draw( Display::Display* display )
    {
        display->clear();
        display->DrawBMP( "/light_large.bmp", 68, 16 ); // 104x104
        display->setTextSize( 2 );
        int16_t x, y;
        const char* str = "LIGHTS ON";
        display->GetTextLocation( str, HorizontalAlignment::Center, VerticalAlignment::Bottom, &x, &y );
        display->setCursor( x, y );
        display->print( str );
    }

    void Discoverable::Draw( Display::Display* display )
    {
        display->clear();
        display->setFont( nullptr );
        std::string message = "Bluetooth Discoverable\nName: " + mBluetoothName;
        int16_t x, y;
        display->GetTextLocation( message.c_str(), HorizontalAlignment::Left, VerticalAlignment::Center, &x, &y );
        display->setCursor( x, y );
        display->print( message.c_str() );
    }

    namespace
    {
        void DrawPair( Display::Display* display, const char* title, const char* value, int x, int y )
        {
            const int line_height = 20;
            display->setCursor( x, y );
            display->setTextSize( 1 );
            display->write( title );
            display->setTextSize( 1 );
            display->setCursor( x, y + line_height );
            display->write( value );
        }
        const char* HeadingToDirection( float heading )
        {
            static const char* directions[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };

            if( heading < 0.0f || heading >= 360.0f )
            {
                return "Invalid";
            }

            int index = ( ( int )( heading + 22.5f ) % 360 ) / 45;
            return directions[ index ];
        }


    }

    void Status::Draw( Display::Display* display )
    {
        display->clear();

        const int x1 = 10;
        const int x2 = 120;
        const int y1 = 15;
        const int y_pitch = 42;

        char buffer[ 128 ];
        char ns = mLatitude > 0 ? 'N' : 'S';
        char ew = mLongitude > 0 ? 'E' : 'W';
        sprintf( buffer, "%2.4f %c", abs( mLatitude ), ns );
        DrawPair( display, "Latitude", buffer, x1, y1 );

        sprintf( buffer, "%3.4f %c", abs( mLongitude ), ew );
        DrawPair( display, "Longitude", buffer, x2, y1 );

        sprintf( buffer, "%3.1f mph", mSpeedMph );
        DrawPair( display, "Speed", buffer, x1, y1 + y_pitch );

        DrawPair( display, "Heading", HeadingToDirection( mHeadingDegrees ), x2, y1 + y_pitch );

        sprintf( buffer, "%2.1f V", mBatteryVolts );
        DrawPair( display, "Battery", buffer, x1, y1 + y_pitch * 2 );
    }

    namespace QuarterMile
    {
        void Start::Draw( Display::Display* display )
        {
            display->clear();
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );
            display->WriteAligned( "Press M to start", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
        }

        void Launch::Draw( Display::Display* display )
        {
            display->clear();
            Display::Rect top_region;
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top, nullptr,
                                   &top_region );

            Display::Rect second_region;
            second_region.x = 0;
            second_region.y = top_region.h + top_region.y + 10;
            second_region.w = 240;
            second_region.h = 135 - second_region.y;
            display->WriteAligned( "Waiting for launch...", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top,
                                   &second_region );
            char buffer[ 128 ];
            sprintf( buffer, "%.1f g", mAccelerationG );
            display->WriteAligned( buffer, Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
        }

        namespace
        {
            void DrawProgressBar( Display::Display* display, uint16_t y, double percentage )
            {
                uint16_t h = 7;
                uint16_t w = 220;

                uint16_t bar_w = percentage * 220;
                display->fillRect( 10, 120, w, h, 0xFFFF );
                display->fillRect( 10, 120, bar_w, h, 0xF800 );
                display->fillRect( bar_w + 10, 120 - 4, 2, h + 8, 0xF800 );
            }
        }

        void InProgress::Draw( Display::Display* display )
        {
            display->clear();
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

            const int x1 = 10;
            const int x2 = 120;
            const int y1 = 40;
            const int y_pitch = 46;

            char buffer[ 128 ];
            sprintf( buffer, "%.1f s", mTimeSec );
            DrawPair( display, "Time", buffer, x1, y1 );

            sprintf( buffer, "%.1f mi", mDistanceMiles );
            DrawPair( display, "Distance", buffer, x2, y1 );

            sprintf( buffer, "%.1f g", mAccelerationG );
            DrawPair( display, "Accel", buffer, x1, y1 + y_pitch );

            sprintf( buffer, "%.1f mph", mSpeedMph );
            DrawPair( display, "Speed", buffer, x2, y1 + y_pitch );

            DrawProgressBar( display, 120, 0.66 );
        }

        void Summary::Draw( Display::Display* display )
        {
            display->clear();
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

            const int x1 = 10;
            const int x2 = 130;
            const int y1 = 45;
            const int y_pitch = 55;

            char buffer[ 128 ];
            sprintf( buffer, "%.1f s", mQuarterMileTimeSec );
            DrawPair( display, "1/4 mi", buffer, x1, y1 );

            sprintf( buffer, "%.1f s", mZeroSixtyTimeSec );
            DrawPair( display, "0-60", buffer, x2, y1 );

            sprintf( buffer, "%.1f mph", mMaxSpeedMph );
            DrawPair( display, "Max Speed", buffer, x1, y1 + y_pitch );

            sprintf( buffer, "%.1f g", mMaxAccelerationG );
            DrawPair( display, "Max Accel", buffer, x2, y1 + y_pitch );
        }
    }
}