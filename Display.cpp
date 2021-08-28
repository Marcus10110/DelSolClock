#include "Display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr int16_t ScreenWidth = 128; // OLED display width, in pixels
constexpr int16_t ScreenHeight = 32; // OLED display height, in pixels
constexpr uint16_t DefaultTextColor = SSD1306_WHITE;
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 gDisplay( ScreenWidth, ScreenHeight, &Wire, -1 );

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
constexpr uint8_t FontSize = 2;

namespace Display
{
    void Begin()
    {
        if( !gDisplay.begin( SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS ) )
        {
            Serial.println( F( "SSD1306 allocation failed" ) );
            for( ;; )
                ; // Don't proceed, loop forever
        }

        gDisplay.clearDisplay();
        gDisplay.display();
    }

    void Clear()
    {
        gDisplay.clearDisplay();
        gDisplay.display();
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
        int16_t x1, y1;
        uint16_t w, h;
        gDisplay.setTextSize( FontSize );
        gDisplay.setTextColor( DefaultTextColor );
        gDisplay.getTextBounds( display_string, 0, 0, &x1, &y1, &w, &h );

        // avoid integer promotion to unsigned.
        int16_t left = ( ScreenWidth - ( int16_t )w ) / 2 + x1;
        int16_t top = ( ScreenHeight - ( int16_t )h ) / 2 + y1;
        gDisplay.setCursor( left, top );
        gDisplay.print( display_string );
        gDisplay.display();
        // center text.
    }

    void DrawSpeed( float speed )
    {
    }

    void DrawLightsIndicator( bool on )
    {
    }
}