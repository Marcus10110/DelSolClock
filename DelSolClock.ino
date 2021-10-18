#include "AppleMediaService.h"
#include "CurrentTimeService.h"
#include "AppleNotificationCenterService.h"
#include "Display.h"
#include "pins.h"
#include "CarIO.h"
#include "Bluetooth.h"
#include "Gps.h"
#include "BleOta.h"
#include <time.h>
#include <sys/time.h>
#include <TinyGPSPlus.h>

// define this when targeting a Adafruit Feather board, instead of a Del Sol Clock. Useful for testing BLE.
#define DEVKIT_ONLY 1
namespace
{
    bool FwUpdateInProgress = false;
    bool NeverConnected = true;
    bool LightsAlarmActive = false;
    std::string StatusCharacteristicValue = "";
}

void setup()
{
    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );

    // Required before we can handle the lights-only power mode.
    CarIO::Setup();
    Display::Begin();
    Gps::Begin();


    // go back to sleep if the car is off, or go into the alarm mode if the lights are on.
    HandlePowerState( CarIO::GetStatus() );

    // if the ignition is off, we wait until it's on, or we go to sleep.
    while( LightsAlarmActive )
    {
        // delay( 10 );
        HandlePowerState( CarIO::GetStatus() );
        CarIO::Service();
    }

    Display::DrawSplash();

    Gps::Wake();
    Bluetooth::Begin();

    delay( 4000 ); // Keep OldSols logo on screen.

    Bluetooth::Service(); // service once to see if we're already connected!
    if( !Bluetooth::IsConnected() )
    {
        // if we're still not connected, display the connection message. If we still have correct time stored in the RTC system, show time
        // too.
        Display::Clear();
        bool is_time_set = Bluetooth::IsTimeSet();
        if( is_time_set )
        {
            DrawCurrentTime();
        }
        Display::DrawDebugInfo( "Bluetooth Discoverable.\nName: DelSolClock", !is_time_set, false );
    }
}

void HandlePowerState( const CarIO::CarStatus& car_status )
{
#ifdef DEVKIT_ONLY
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
            Display::Clear();
            Display::DrawLightAlarm();
            CarIO::StartBeeper( 4, 493, 50, 125, 3000 );
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
    Display::EnableSleep( sleep );
    gpio_hold_en( static_cast<gpio_num_t>( Pin::TftLit ) ); // hold 0 while sleeping.
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
    HandlePowerState( car_status );
    HandleStatusUpdate( car_status );
    if( LightsAlarmActive )
    {
        return;
    }

    if( NeverConnected && Bluetooth::IsConnected() )
    {
        NeverConnected = false;
    }

    if( NeverConnected && !Bluetooth::IsConnected() )
    {
        bool update = false;
        bool is_time_set = Bluetooth::IsTimeSet();

        tm time;
        if( is_time_set && getLocalTime( &time, 100 ) )
        {
            if( Display::GetState().mHours24 != time.tm_hour || Display::GetState().mMinutes != time.tm_min )
            {
                update = true;
            }
        }


        if( car_status.mLights != Display::GetState().mIconHeadlight )
        {
            update = true;
        }
        if( update )
        {
            Display::Clear();
            Display::DrawIcon( Display::Icon::Headlight, car_status.mLights );
            if( is_time_set )
            {
                DrawCurrentTime();
            }
            Display::DrawDebugInfo( "Bluetooth On\n  Name: \"Del Sol\"", !is_time_set, false );
        }
        delay( 100 );
        return;
    }


    Display::DisplayState new_state;
    tm time;
    if( Bluetooth::IsTimeSet() && getLocalTime( &time, 100 ) )
    {
        new_state.mHours24 = time.tm_hour;
        new_state.mMinutes = time.tm_min;
    }
    // TODO: GPS updates quite a bit, we should consider hanving a seperate regional clear for this.
    if( Gps::GetGps()->speed.isUpdated() )
    {
        new_state.mSpead = Gps::GetGps()->speed.mph();
        if( new_state.mSpead < 10 )
        {
            new_state.mSpead = 0;
        }
    }

    new_state.mIconHeadlight = car_status.mLights;
    new_state.mIconBluetooth = Bluetooth::IsConnected();

    const auto& media_info = AppleMediaService::GetMediaInformation();
    new_state.mIconShuffle = media_info.mShuffleMode != AppleMediaService::MediaInformation::ShuffleMode::Off;
    new_state.mIconRepeat = media_info.mRepeatMode != AppleMediaService::MediaInformation::RepeatMode::Off;
    if( media_info.mPlaybackState == AppleMediaService::MediaInformation::PlaybackState::Playing )
    {
        new_state.mMediaArtist = media_info.mArtist;
        new_state.mMediaTitle = media_info.mTitle;
    }
    if( Display::GetState() != new_state )
    {
        Serial.println( "Display state different! Current:" );
        Display::GetState().Dump();
        Serial.println( "New:" );
        new_state.Dump();
        Serial.println( "" );
        Display::DrawState( new_state );
    }
    delay( 100 );
}

void DrawCurrentTime()
{
    tm time;
    if( getLocalTime( &time, 100 ) )
    {
        Display::DrawTime( time.tm_hour, time.tm_min );
    }
    else
    {
        Serial.println( "failed to get time" );
    }
}
