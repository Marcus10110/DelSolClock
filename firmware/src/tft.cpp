#include "tft.h"
#include <Arduino.h>
#include <SPI.h>
#include "pins.h"
#include <Adafruit_GFX.h>
#include "SPIFFS.h"
#include "logger.h"
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
        LOG_TRACE( "Tft::Init()" );
        digitalWrite( Pin::TftPower, 1 );
        delay( 100 );
        mTft.init( 136, 240 );
        mTft.setRotation( 3 );
        mTft.fillScreen( 0 );

        if( !SPIFFS.begin() )
        {
            LOG_ERROR( "SPIFFS failed" );
        }
    }

    void Tft::EnableSleep( bool sleep )
    {
        // TODO: power down / power up TFT.
        mTft.enableSleep( sleep );
        if( sleep )
        {
            // TODO: what should we do here?
            // gpio_hold_en( static_cast<gpio_num_t>( Pin::TftPower ) ); // hold 0 while sleeping.
        }
    }

    void Tft::DrawCanvas( GFXcanvas16* canvas )
    {
        mTft.drawRGBBitmap( 0, 0, canvas->getBuffer(), canvas->width(), canvas->height() );
    }
}