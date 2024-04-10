#include <Arduino.h>
#include "apple_media_service.h"
#include "current_time_service.h"
#include "apple_media_service.h"
#include "tft.h"
#include "display.h"
#include "screens.h"
#include "pins.h"
#include "car_io.h"
#include "bluetooth.h"
#include "gps.h"
#include "ble_ota.h"
#include "motion.h"
#include "demo.h"
#include "quarter_mile.h"

#include <TinyGPSPlus.h>

#include <time.h>
#include <sys/time.h>


enum class CurrentScreen
{
    Default, // clock, OTA, splash, etc.
    Status,
    QM,
};


// define this when targeting a Adafruit Feather board, instead of a Del Sol Clock. Useful for testing BLE.
#define DISABLE_SLEEP
// define this to run the demo mode, which will cycle through all the screens.
// #define DEMO_MODE
namespace
{
    bool FwUpdateInProgress = false;
    bool LightsAlarmActive = false;
    std::string StatusCharacteristicValue = "";
    const std::string BluetoothDeviceName = "Del Sol";

    Tft::Tft* gTft;
    Display::Display gDisplay;
    CurrentScreen gCurrentScreen = CurrentScreen::Default;
}

void HandlePowerState( const CarIO::CarStatus& car_status );
void Sleep();


void setup()
{
    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );


    // Required before we can handle the lights-only power mode.
    CarIO::Setup();
    gTft = new Tft::Tft();
    gTft->Init();
    Gps::Begin();
    Motion::Begin();

    {
        // write the splash screen to the display buffer before starting BLE. this uses a huge amount of RAM temporarily.
        Screens::Splash splash;
        splash.Draw( &gDisplay );
        gTft->DrawCanvas( &gDisplay );
    }

    Serial.println( "setup, bluetooth begin..." );
    Bluetooth::Begin( BluetoothDeviceName );
    // leave splash screen up for a few seconds.
    delay( 3000 );


#ifdef DEMO_MODE
    while( 1 )
    {
        Demo::Demo( &gDisplay, gTft );
    }
#endif

    // go back to sleep if the car is off, or go into the alarm mode if the lights are on.
    Serial.println( "setup, HandlePowerState..." );
    HandlePowerState( CarIO::GetStatus() );

    // if the ignition is off, we wait until it's on, or we go to sleep.
    while( LightsAlarmActive )
    {
        HandlePowerState( CarIO::GetStatus() );
        CarIO::Service();
    }
    Serial.println( "setup, GPS wake..." );
    Gps::Wake();

    Serial.println( "setup, bluetooth service..." );
    Bluetooth::Service(); // service once to see if we're already connected!
    if( !Bluetooth::IsConnected() )
    {
        // if we're still not connected, display the connection message. If we still have correct time stored in the RTC system, show time
        // too.
        Serial.println( "setting up to write bluetooth discoverable" );

        Screens::Discoverable discoverable;
        discoverable.mBluetoothName = BluetoothDeviceName;
        discoverable.Draw( &gDisplay );
        gTft->DrawCanvas( &gDisplay );
        delay( 1000 );
    }
}

void HandlePowerState( const CarIO::CarStatus& car_status )
{
#ifdef DISABLE_SLEEP
    return;
#endif
    // if IGN and Illumination are off, go to sleep.
    if( !car_status.mIgnition && !car_status.mLights )
    {
        Serial.println( "Ignition and lights are off. Going to sleep." );
        Sleep();
        return;
    }

    // If IGN is gone but Illumination is ON, display lights alarm.
    if( !car_status.mIgnition && car_status.mLights )
    {
        if( !LightsAlarmActive )
        {
            Serial.println( "Ignition is off, but Lights are on. Alarm" );
            LightsAlarmActive = true;
            CarIO::StartBeeper( 4, 1100, 80, 125, 1600 );
            Screens::LightsAlarm lights_alarm;
            lights_alarm.Draw( &gDisplay );
            gTft->DrawCanvas( &gDisplay );
        }
        return;
    }

    // If IGN is present, return to normal behavior.
    if( car_status.mIgnition )
    {
        if( LightsAlarmActive )
        {
            Serial.println( "Ignition on, turning off the lights alarm." );
            LightsAlarmActive = false;
            CarIO::StopBeeper();
        }
    }
}

void Sleep()
{
    Serial.println( "going to sleep..." );
    Gps::Sleep();
    gTft->EnableSleep( true );
    esp_sleep_enable_ext1_wakeup( ( 1ull << Pin::Ignition ) | ( 1ull << Pin::Illumination ), ESP_EXT1_WAKEUP_ANY_HIGH );
    Serial.flush();
    delay( 5 );
    esp_deep_sleep_start();
}

void HandleStatusUpdate( const CarIO::CarStatus& car_status )
{
    // StatusCharacteristicValue
    char update[ 128 ] = { 0 };
    snprintf( update, sizeof( update ), "%i,%i,%i,%i,%i", car_status.mRearWindow, car_status.mTrunk, car_status.mRoof, car_status.mIgnition,
              car_status.mLights );
    if( strcmp( update, StatusCharacteristicValue.c_str() ) != 0 )
    {
        StatusCharacteristicValue = update;
        Serial.printf( "updating BLE characteristic with %s\n", StatusCharacteristicValue.c_str() );
        Bluetooth::SetVehicleStatus( StatusCharacteristicValue );
    }
}


void loop()
{
    uint32_t fw_bytes = 0;
    if( BleOta::IsInProgress( &fw_bytes ) )
    {
        if( !FwUpdateInProgress )
        {
            FwUpdateInProgress = true;
            // update the display
            Serial.println( "FW Update started. Disabling normal functions." );
        }
        return; // don't do anything else while we're updating.
    }
    Bluetooth::Service();
    Gps::Service();
    CarIO::Service();
    auto car_status = CarIO::GetStatus();
    auto button_events = CarIO::GetButtonEvents();
    if( button_events.mHourButtonPressed )
    {
        Serial.println( "hour button clicked" );
        // advance current screen by one.
        if( gCurrentScreen == CurrentScreen::Default )
        {
            gCurrentScreen = CurrentScreen::Status;
        }
        else if( gCurrentScreen == CurrentScreen::Status )
        {
            gCurrentScreen = CurrentScreen::QM;
            QuarterMile::Reset();
        }
        else if( gCurrentScreen == CurrentScreen::QM )
        {
            gCurrentScreen = CurrentScreen::Default;
        }
    }
    if( button_events.mMinuteButtonPressed )
    {
        Serial.println( "minute button clicked" );
    }
    HandlePowerState( car_status );
    HandleStatusUpdate( car_status );
    if( LightsAlarmActive )
    {
        return;
    }

    if( gCurrentScreen == CurrentScreen::Default )
    {
        // update the display with everything.
        Screens::Clock clock;
        tm time;
        if( getLocalTime( &time, 100 ) )
        {
            clock.mHours24 = time.tm_hour;
            clock.mMinutes = time.tm_min;
        }

        if( Gps::GetGps()->speed.isValid() && Gps::GetGps()->speed.age() < 60000 )
        {
            clock.mSpeed = Gps::GetGps()->speed.mph();
        }
        clock.mHeadlight = car_status.mLights;
        clock.mBluetooth = Bluetooth::IsConnected();
        const auto& media_info = AppleMediaService::GetMediaInformation();
        if( media_info.mPlaybackState == AppleMediaService::MediaInformation::PlaybackState::Playing )
        {
            clock.mMediaArtist = media_info.mArtist;
            clock.mMediaTitle = media_info.mTitle;
        }

        clock.Draw( &gDisplay );
        gTft->DrawCanvas( &gDisplay );
    }
    else if( gCurrentScreen == CurrentScreen::Status )
    {
        Screens::Status status;
        status.mBatteryVolts = car_status.mBatteryVoltage;
        if( Gps::GetGps()->location.isValid() )
        {
            status.mLatitude = Gps::GetGps()->location.lat();
            status.mLongitude = Gps::GetGps()->location.lng();
            status.mSpeedMph = Gps::GetGps()->speed.mph();
            status.mHeadingDegrees = Gps::GetGps()->course.deg();
        }
        auto motion = Motion::GetState();
        if( motion.mValid )
        {
            status.mForwardG = motion.mForward;
            status.mLateralG = motion.mLeft;
            status.mVerticalG = motion.mUp;
        }
        status.Draw( &gDisplay );
        gTft->DrawCanvas( &gDisplay );
    }
    else if( gCurrentScreen == CurrentScreen::QM )
    {
        QuarterMile::Service( &gDisplay, gTft, button_events, Gps::GetGps() );
    }
    delay( 10 );
}
