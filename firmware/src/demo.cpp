#include "demo.h"
#include "screens.h"
#include "tft.h"
#include "display.h"

namespace Demo
{
    namespace
    {
        void DrawScreen( Screens::Screen& screen, Display::Display* display, Tft::Tft* tft, int ms = 3000 )
        {
            Serial.println( "DrawScreen" );
            screen.Draw( display );
            tft->DrawCanvas( display );
            delay( ms );
        }
    }

    void Demo( Display::Display* display, Tft::Tft* tft )
    {
        {
            Screens::OtaInProgress ota;

            for( uint32_t i = 0; i < 10000; i += 99 )
            {
                ota.mBytesReceived = i;
                DrawScreen( ota, display, tft, 15 );
            }
        }

        {
            Screens::Discoverable discoverable;
            discoverable.mBluetoothName = "Del Sol";
            DrawScreen( discoverable, display, tft );
        }

        {
            Screens::Splash splash;
            DrawScreen( splash, display, tft );
        }

        {
            Screens::Clock clock;
            clock.mHours24 = 19;
            clock.mMinutes = 58;
            clock.mBluetooth = true;
            clock.mMediaTitle = "Clocks";
            clock.mMediaArtist = "Coldplay";
            clock.mSpeed = 88.1;
            clock.mHeadlight = true;
            DrawScreen( clock, display, tft );
        }

        {
            Screens::LightsAlarm lights_alarm;
            DrawScreen( lights_alarm, display, tft );
        }

        {
            Screens::Status status;
            status.mLatitude = 37.7511596;
            status.mLongitude = -122.4561478;
            status.mSpeedMph = 88.1;
            status.mBatteryVolts = 13.5;
            status.mHeadingDegrees = 133;
            DrawScreen( status, display, tft );
        }

        {
            Screens::QuarterMile::Start qm1;
            DrawScreen( qm1, display, tft );
        }

        {
            Screens::QuarterMile::Launch qm2;
            qm2.mAccelerationG = 0.1;
            DrawScreen( qm2, display, tft );
        }

        {
            Screens::QuarterMile::InProgress qm3;
            qm3.mTimeSec = 1;
            qm3.mAccelerationG = 0.1;
            qm3.mSpeedMph = 1;
            qm3.mDistanceMiles = 0.1;
            for( float i = 0; i <= 0.25; i += 0.0025 )
            {
                qm3.mDistanceMiles = i;
                qm3.mSpeedMph = ( i / 0.25 ) * 119;
                qm3.mTimeSec = ( i / 0.25 ) * 14.1;
                DrawScreen( qm3, display, tft, 40 );
            }
        }

        {
            Screens::QuarterMile::Summary qm4;
            qm4.mMaxAccelerationG = 0.9;
            qm4.mMaxSpeedMph = 121.1;
            qm4.mZeroSixtyTimeSec = 9.9;
            qm4.mQuarterMileTimeSec = 87.1;
            DrawScreen( qm4, display, tft );
        }
    }
}