#include "screens.h"

#include <Arduino.h>
#include <Adafruit_GFX.h>

#include <SPI.h>
#include <SPIFFS_ImageReader.h>
#include <string>

// the adafruit font converter converts fonts at 141 DPI.
// pixel size = pt size / 72 * 141.
#include "fonts/digital_7__mono_40pt7b.h"
#include "fonts/JetBrainsMono_Thin7pt7b.h"
#include "fonts/JetBrainsMono_Thin8pt7b.h"
#include "fonts/JetBrainsMono_Thin9pt7b.h"
#include "fonts/JetBrainsMono_Thin10pt7b.h" // Capital N is 14px tall.
#include "fonts/JetBrainsMono_Thin12pt7b.h"
#include "fonts/JetBrainsMono_Thin14pt7b.h" // Capital N is 20px tall.
#include "fonts/JetBrainsMono_Thin16pt7b.h"

using namespace Display;

namespace Screens
{
    void Splash::Draw( Display::Display* display )
    {
        display->clear();
        display->DrawBMP( "/OldSols.bmp", 0, 0, true );
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
        display->setFont( &digital_7__mono_40pt7b );
        display->setTextSize( 1 );
        display->setTextWrap( false ); // disable wrap just for this function.
        int16_t x, y;
        Rect time_region;
        display->GetTextLocation( display_string, HorizontalAlignment::Center, VerticalAlignment::Center, &x, &y, nullptr, &time_region );
        display->setCursor( x, y );
        display->print( display_string );

        // Draw AM / PM indicator
        display->setFont( &JetBrainsMono_Thin8pt7b );
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
        display->setFont( &JetBrainsMono_Thin14pt7b );
        display->setTextWrap( false );
        display->GetTextLocation( display_string, HorizontalAlignment::Left, VerticalAlignment::Bottom, &x, &y );
        display->setCursor( x, y );
        display->print( display_string );
        display->setTextWrap( true );


        // speed
        if( mSpeed > 10 )
        {
            snprintf( display_string, sizeof( display_string ), "%.0f", mSpeed );
            char mph[] = "MPH";
            display->setFont( &JetBrainsMono_Thin12pt7b );
            display->WriteAligned( display_string, HorizontalAlignment::Left, VerticalAlignment::Top );

            // add MPH
            display->setFont( &JetBrainsMono_Thin8pt7b );
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
        display->setFont( &JetBrainsMono_Thin16pt7b );
        display->setTextSize( 1 );
        int16_t x, y;
        const char* str = "LIGHTS ON";
        display->GetTextLocation( str, HorizontalAlignment::Center, VerticalAlignment::Bottom, &x, &y );
        display->setCursor( x, y );
        display->print( str );
    }

    void Discoverable::Draw( Display::Display* display )
    {
        display->clear();
        display->setFont( &JetBrainsMono_Thin9pt7b );
        std::string message = "Bluetooth Discoverable\nName: " + mBluetoothName;
        int16_t x, y;
        display->GetTextLocation( message.c_str(), HorizontalAlignment::Left, VerticalAlignment::Center, &x, &y );
        display->setCursor( x, y );
        display->print( message.c_str() );
    }

    namespace
    {
        void DrawPair( Display::Display* display, const char* title, const char* value, int x, int y, bool tiny_value = false )
        {
            const int line_height = 20;

            display->setFont( &JetBrainsMono_Thin7pt7b );
            display->setCursor( x, y );
            display->write( title );
            if( !tiny_value )
            {
                display->setFont( &JetBrainsMono_Thin10pt7b );
            }
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
        snprintf( buffer, sizeof( buffer ), "%2.4f %c", std::abs( mLatitude ), ns );
        DrawPair( display, "Latitude", buffer, x1, y1 );

        snprintf( buffer, sizeof( buffer ), "%3.4f %c", std::abs( mLongitude ), ew );
        DrawPair( display, "Longitude", buffer, x2, y1 );

        snprintf( buffer, sizeof( buffer ), "%3.1f mph", mSpeedMph );
        DrawPair( display, "Speed", buffer, x1, y1 + y_pitch );

        DrawPair( display, "Heading", HeadingToDirection( mHeadingDegrees ), x2, y1 + y_pitch );

        snprintf( buffer, sizeof( buffer ), "%2.1f V", mBatteryVolts );
        DrawPair( display, "Battery", buffer, x1, y1 + y_pitch * 2 );

        snprintf( buffer, sizeof( buffer ), "% 1.1f/% 1.1f/% 1.1f", mForwardG, mLateralG, mVerticalG );
        DrawPair( display, "G-Force", buffer, x2, y1 + y_pitch * 2, true );
    }

    void OtaInProgress::Draw( Display::Display* display )
    {
        char buffer[ 128 ];
        snprintf( buffer, sizeof( buffer ), "%u Bytes", mBytesReceived );

        display->clear();
        display->setFont( &JetBrainsMono_Thin14pt7b );
        display->WriteAligned( "Updating", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );
        display->setFont( &JetBrainsMono_Thin10pt7b );
        display->WriteAligned( buffer, Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
    }

    void Notifications::Draw( Display::Display* display )
    {
        display->clear();
        if( !mHasNotification )
        {
            display->setFont( &JetBrainsMono_Thin14pt7b );
            display->WriteAligned( "Notifications", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );
            display->setFont( &JetBrainsMono_Thin10pt7b );
            display->WriteAligned( "No Notifications", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
            return;
        }

        display->setFont( &JetBrainsMono_Thin14pt7b );
        display->WriteAligned( "Notifications", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

        String summary = mNotification.mTitle + "\n" + mNotification.mSubtitle + "\n" + mNotification.mMessage;

        display->setFont( &JetBrainsMono_Thin7pt7b );
        display->WriteAligned( summary.c_str(), Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
    }

    void Navigation::Draw( Display::Display* display )
    {
        display->clear();

        display->setFont( &JetBrainsMono_Thin14pt7b );
        display->WriteAligned( "Navigation", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

        if( !mHasNotification )
        {
            display->setFont( &JetBrainsMono_Thin10pt7b );
            display->WriteAligned( "No Notifications", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
            return;
        }

        const char* left_prefix = "Turn left onto";
        const char* right_prefix = "Turn right onto";
        bool is_left = mNotification.mMessage.startsWith( left_prefix );
        bool is_right = mNotification.mMessage.startsWith( right_prefix );

        if( !is_left && !is_right )
        {
            // display the contents in the center.
            if( mNotification.mMessage.length() <= 60 )
            {
                display->setFont( &JetBrainsMono_Thin10pt7b );
            }
            else if( mNotification.mMessage.length() <= 80 )
            {
                display->setFont( &JetBrainsMono_Thin9pt7b );
            }
            else
            {
                display->setFont( &JetBrainsMono_Thin8pt7b );
            }
            display->WriteAligned( mNotification.mMessage.c_str(), Display::HorizontalAlignment::Center,
                                   Display::VerticalAlignment::Center );
            return;
        }

        // draw an arrow on the right side of the screen.
        const int bmp_size = 80; // 80x80 pixels
        auto screen_rect = display->ScreenRect();
        int top_height = 20;
        screen_rect.y += top_height;
        screen_rect.h -= top_height;
        if( is_left )
        {
            display->DrawBMP( "/left.bmp", screen_rect.x + screen_rect.w - bmp_size, screen_rect.y + ( screen_rect.h - bmp_size ) / 2 );
        }
        else if( is_right )
        {
            display->DrawBMP( "/right.bmp", screen_rect.x + screen_rect.w - bmp_size, screen_rect.y + ( screen_rect.h - bmp_size ) / 2 );
        }
        // write the text on the left side of the screen.
        String line1 = is_left ? left_prefix : right_prefix;
        String line2 = mNotification.mMessage.substring( is_left ? strlen( left_prefix ) : strlen( right_prefix ) );
        line2.trim();
        // line 1 in 8pt, line 2 in 10pt.
        display->setFont( &JetBrainsMono_Thin8pt7b );
        display->setCursor( 0, top_height + 26 );
        display->write( line1.c_str() );
        display->write( "\n" );
        // display->WriteAligned( line1.c_str(), Display::HorizontalAlignment::Left, Display::VerticalAlignment::Center );

        display->setFont( &JetBrainsMono_Thin10pt7b );
        display->write( line2.c_str() );
    }

    namespace QuarterMile
    {
        void Start::Draw( Display::Display* display )
        {
            display->clear();
            display->setFont( &JetBrainsMono_Thin14pt7b );
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );
            display->setFont( &JetBrainsMono_Thin10pt7b );
            display->WriteAligned( "Press M to Start", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Center );
        }

        void Launch::Draw( Display::Display* display )
        {
            display->clear();
            Display::Rect top_region;
            display->setFont( &JetBrainsMono_Thin14pt7b );
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top, nullptr,
                                   &top_region );

            display->setFont( &JetBrainsMono_Thin8pt7b );
            Display::Rect second_region;
            second_region.x = 0;
            second_region.y = top_region.h + top_region.y + 10;
            second_region.w = 240;
            second_region.h = 135 - second_region.y;
            display->WriteAligned( "Waiting for Launch", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top,
                                   &second_region );
            char buffer[ 128 ];
            snprintf( buffer, sizeof( buffer ), "%.1f g", mAccelerationG );
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
            display->setFont( &JetBrainsMono_Thin14pt7b );
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

            const int x1 = 10;
            const int x2 = 120;
            const int y1 = 40;
            const int y_pitch = 46;
            char buffer[ 128 ];
            snprintf( buffer, sizeof( buffer ), "%.1f s", mTimeSec );
            DrawPair( display, "Time", buffer, x1, y1 );

            snprintf( buffer, sizeof( buffer ), "%.2f mi", mDistanceMiles );
            DrawPair( display, "Distance", buffer, x2, y1 );

            snprintf( buffer, sizeof( buffer ), "%.1f g", mAccelerationG );
            DrawPair( display, "Accel", buffer, x1, y1 + y_pitch );

            snprintf( buffer, sizeof( buffer ), "%.1f mph", mSpeedMph );
            DrawPair( display, "Speed", buffer, x2, y1 + y_pitch );
            double distance = mDistanceMiles;
            if( distance < 0 )
            {
                distance = 0;
            }
            if( distance > 0.25 )
            {
                distance = 0.25;
            }
            DrawProgressBar( display, 120, distance / 0.25 );
        }

        void Summary::Draw( Display::Display* display )
        {
            display->clear();
            display->setFont( &JetBrainsMono_Thin14pt7b );
            display->WriteAligned( "Quarter Mile", Display::HorizontalAlignment::Center, Display::VerticalAlignment::Top );

            const int x1 = 10;
            const int x2 = 130;
            const int y1 = 45;
            const int y_pitch = 55;

            char buffer[ 128 ];
            snprintf( buffer, sizeof( buffer ), "%.1f s", mQuarterMileTimeSec );
            DrawPair( display, "1/4 mi", buffer, x1, y1 );

            snprintf( buffer, sizeof( buffer ), "%.1f s", mZeroSixtyTimeSec );
            DrawPair( display, "0-60", buffer, x2, y1 );

            snprintf( buffer, sizeof( buffer ), "%.1f mph", mMaxSpeedMph );
            DrawPair( display, "Max Speed", buffer, x1, y1 + y_pitch );

            snprintf( buffer, sizeof( buffer ), "%.1f g", mMaxAccelerationG );
            DrawPair( display, "Max Accel", buffer, x2, y1 + y_pitch );
        }
    }
}