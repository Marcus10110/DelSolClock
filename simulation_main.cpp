#if 0

#include "simulation_main.h"

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"


#include "screens.h"

#include <SdFat.h>
#include <Adafruit_SPIFlash.h>    // SPI / QSPI flash library
#include <Adafruit_ImageReader.h> // Image-reading functions

#define TFT_DC 2
#define TFT_CS 15
Adafruit_ILI9341 tft = Adafruit_ILI9341( TFT_CS, TFT_DC );


#define SPI_SPEED SD_SCK_MHZ( 4 )
#define CS_PIN 5
SdFat sd;
Adafruit_ImageReader reader( sd );


Display::Display display;

namespace
{
    void SdImageReader( GFXcanvas16* destination, char* path, int16_t x, int16_t y )
    {
        Adafruit_Image img;

        reader.loadBMP( path, img );
    }
}

void DelSolClockSimulator::Setup()
{
    Serial.begin( 115200 );
    Serial.println( "simulator setup" );
    tft.begin();
    /*
    tft.setCursor( 20, 120 );
    tft.setTextColor( ILI9341_RED );
    tft.setTextSize( 3 );
    tft.println( "Hello ESP32" );

    tft.setCursor( 24, 160 );
    tft.setTextColor( ILI9341_GREEN );
    tft.setTextSize( 2 );
    tft.println( "I can do SPI :-)" );
    */


    if( !sd.begin( CS_PIN, SPI_SPEED ) )
    {
        if( auto code = sd.card()->errorCode() )
        {
            Serial.println( "SD initialization failed." );
            printSdErrorSymbol( &Serial, code );
            Serial.println( "" );
            printSdErrorText( &Serial, code );
            Serial.println( "" );
            Serial.println( ( int )code );
        }
        else if( sd.vol()->fatType() == 0 )
        {
            Serial.println( "Can't find a valid FAT16/FAT32 partition." );
        }
        else
        {
            Serial.println( "Can't determine error type" );
        }
        return;
    }

    Serial.println( "Files on card:" );
    Serial.println( "   Size    Name" );

    sd.ls( LS_R | LS_SIZE );
    Serial.println( "end of LS" );
}

namespace
{
    void DrawScreen( Screens::Screen& screen, int ms = 2000 )
    {
        screen.Draw( &display );
        tft.drawRGBBitmap( 0, 0, display.getBuffer(), display.width(), display.height() );
        tft.drawLine( 0, 135, 240, 135, 0xFFFF );
        delay( ms );
    }
}

void DelSolClockSimulator::Loop()
{
    Screens::Discoverable discoverable;
    discoverable.mBluetoothName = "Del Sol";
    Screens::Splash splash;
    Screens::Clock clock;
    clock.mHours24 = 19;
    clock.mMinutes = 58;
    clock.mBluetooth = true;
    clock.mMediaTitle = "Clocks";
    clock.mMediaArtist = "Coldplay";
    clock.mSpeed = 88.1;
    clock.mHeadlight = true;
    Screens::LightsAlarm lights_alarm;
    Screens::Status status;
    status.mLatitude = 37.7511596;
    status.mLongitude = -122.4561478;
    status.mSpeedMph = 88.1;
    status.mBatteryVolts = 13.5;
    status.mHeadingDegrees = 133;
    Screens::QuarterMile::Start qm1;
    Screens::QuarterMile::Launch qm2;
    qm2.mAccelerationG = 0.1;
    Screens::QuarterMile::InProgress qm3;
    qm3.mTimeSec = 1;
    qm3.mAccelerationG = 0.1;
    qm3.mSpeedMph = 1;
    qm3.mDistanceMiles = 0.1;
    Screens::QuarterMile::Summary qm4;
    qm4.mMaxAccelerationG = 0.9;
    qm4.mMaxSpeedMph = 121.1;
    qm4.mZeroSixtyTimeSec = 9.9;
    qm4.mQuarterMileTimeSec = 87.1;


    DrawScreen( discoverable );
    DrawScreen( splash );
    DrawScreen( clock );
    DrawScreen( lights_alarm );
    DrawScreen( status );
    DrawScreen( qm1 );
    DrawScreen( qm2 );
    DrawScreen( qm3 );
    DrawScreen( qm4 );
}

#endif