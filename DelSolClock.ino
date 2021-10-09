#include "AppleMediaService.h"
#include "CurrentTimeService.h"
#include "AppleNotificationCenterService.h"
#include "Display.h"
#include "pins.h"
#include "CarIO.h"
#include "Bluetooth.h"
#include "Gps.h"
#include <time.h>
#include <sys/time.h>
#include <TinyGPSPlus.h>

namespace
{
    bool TrackNameDirty = false;
    bool NeverConnected = true;
    uint32_t BootFinishedTimeMs = 0;

    RTC_DATA_ATTR int BootCount = 0;
}

void setup()
{
    CarIO::Setup();

    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );

    // Increment boot number and print it every reboot
    ++BootCount;
    Serial.println( "Boot number: " + String( BootCount ) );

    // Print the wakeup reason for ESP32
    PrintWakeupReason();

    Gps::Begin();
    Gps::Wake();

    Display::Begin();

    Bluetooth::Begin();

    AppleMediaService::RegisterForNotifications( []() { TrackNameDirty = true; }, AppleMediaService::NotificationLevel::TrackTitleOnly );

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
        Display::DrawDebugInfo( "Bluetooth Discoverable. Name: DelSolClock", !is_time_set );
    }
    BootFinishedTimeMs = millis();
}


void loop()
{
    bool sleep = millis() - BootFinishedTimeMs > 5000;
    if( sleep )
    {
        Serial.println( "going to sleep..." );
        // let's try sleeping!!
        Gps::Sleep();
        Display::EnableSleep( sleep );
        gpio_hold_en( static_cast<gpio_num_t>( Pin::TftLit ) ); // hold 0 while sleeping.
        esp_sleep_enable_timer_wakeup( 5 * 1000000 );
        Serial.flush();
        delay( 5 );
        esp_deep_sleep_start();
    }
    Bluetooth::Service();
    Gps::Service();
    auto car_status = CarIO::GetStatus();
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
            Display::DrawDebugInfo( "Bluetooth Discoverable. Name: DelSolClock", !is_time_set );
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
    if( Display::GetState() != new_state && !sleep )
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

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void PrintWakeupReason()
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch( wakeup_reason )
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println( "Wakeup caused by external signal using RTC_IO" );
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println( "Wakeup caused by external signal using RTC_CNTL" );
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println( "Wakeup caused by timer" );
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println( "Wakeup caused by touchpad" );
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println( "Wakeup caused by ULP program" );
        break;
    default:
        Serial.printf( "Wakeup was not caused by deep sleep: %d\n", wakeup_reason );
        break;
    }
}