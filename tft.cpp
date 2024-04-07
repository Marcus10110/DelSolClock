#include "tft.h"
#include <Arduino.h>
#include <SPI.h>
#include "pins.h"
#include <Adafruit_GFX.h>
#include "SPIFFS.h"
namespace Tft
{
    namespace
    {
        SPIClass* BuildDisplaySpi()
        {
            SPIClass* vspi = new SPIClass( VSPI );
            vspi->begin( Pin::Sck, Pin::Miso, Pin::Mosi, Pin::TftCs );
            return vspi;
        }
    }
    Tft::Tft() : mTft( BuildDisplaySpi(), Pin::TftCs, Pin::TftDc, Pin::TftReset )
    {
    }

    void Tft::Init()
    {
        digitalWrite( Pin::TftPower, 1 );
        delay( 100 );
        mTft.init( 136, 240 );
        mTft.setRotation( 3 );
        mTft.fillScreen( 0 );

        if( !SPIFFS.begin() )
        {
            Serial.println( "SPIFFS failed" );
        }
    }

    void Tft::EnableSleep( bool sleep )
    {
        // TODO: power down / power up TFT.
        mTft.enableSleep( sleep );
    }

    void Tft::DrawCanvas( GFXcanvas16* canvas )
    {
        mTft.drawRGBBitmap( 0, 0, canvas->getBuffer(), canvas->width(), canvas->height() );
    }
}