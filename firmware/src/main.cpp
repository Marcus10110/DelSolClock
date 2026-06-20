

#include <Arduino.h>
#include "apple_media_service.h"
#if ENABLE_APPLE_NOTIFICATIONS
#include "apple_notification_center_service.h"
#include "notification_processor.h"
#endif
#include "current_time_service.h"
#include "apple_media_service.h"
#include "tft.h"
#include "draw_helpers.h"
#include "clock_screen.h"
#include "simple_screens.h"
#include "status_screen.h"
#include "notification_screens.h"
#include "gmeter_screen.h"
#include "image_loader.h"
#include "pins.h"
#include "car_io.h"
#include "bluetooth.h"
#include "gps.h"
#include "ble_ota.h"
#include "motion.h"
#include "demo.h"
#include "quarter_mile.h"
#include "navigation_service.h"
#include "matcher.h"
#include "logger.h"
#include "debug_service.h"
#include "utilities.h"

#include <TinyGPSPlus.h>

#include <time.h>
#include <sys/time.h>

#include <esp_task_wdt.h>

#include "soc/rtc.h"


enum class CurrentScreen
{
    Default, // clock, OTA, splash, etc.
    Status,
    QM,
    Notifications,
    Navigation,
    GMeter,
};


// define this when targeting a Adafruit Feather board, instead of a Del Sol Clock. Useful for testing BLE.
#define DISABLE_SLEEP
// define this to run the demo mode, which will cycle through all the screens.
// #define DEMO_MODE
namespace
{
    bool FwUpdateInProgress = false;
    uint32_t FwBytesReceived = 0;
    bool LightsAlarmActive = false;
    const std::string BluetoothDeviceName = "Del Sol";

    Tft::Tft* gTft;
    GFXcanvas16 gDisplay( display::kWidth, display::kHeight );
    CurrentScreen gCurrentScreen = CurrentScreen::Default;

    // Latest navigation match result, updated by the live matcher when a route is
    // loaded + GPS is fresh. Consumed by the Navigation screen render.
    nav::MatchResult gNavResult;
    bool gNavHasFix = false;        // GPS had a fresh fix at last match
    std::string gNavInstruction;    // next-maneuver instruction (owns the string)

    Motion::HistoryTracker BrakeHistoryTracker( display::GMeterProps::HistorySize, 100 );
    Motion::HistoryTracker LateralHistoryTracker( display::GMeterProps::HistorySize, 100 );
}

void HandlePowerState( const CarIO::CarStatus& car_status );
void Sleep();


void setup()
{
    Serial.begin( 115200 );
    LOG_TRACE( "Del Sol Clock Booting" );

    Serial.println( "Configuring WDT..." );
    esp_task_wdt_init( 50, true ); // huge because I have not done a great job of resetting the watchdog timer.
    esp_task_wdt_add( NULL );

    PRINT_MEMORY_USAGE();

    // show the current clock source. If it's not 1 (the external 32kHz crystal), then either (A) the crystal is not present, or (B) the
    // bootloader & esp32-arduino library was not configured for it.
    auto src = rtc_clk_slow_freq_get();
    auto hz = rtc_clk_slow_freq_get_hz();
    LOG_TRACE( "RTC_SLOW_CLK frequency: %u Hz, source: %i", hz, src );

    // Required before we can handle the lights-only power mode.
    CarIO::Setup();
    gTft = new Tft::Tft();
    gTft->Init();
    Gps::Begin();
    Motion::Begin();
    {
        const char* images_to_preload[] = { "/bluetooth.bmp", "/light_small.bmp", "/light_large.bmp", "/left.bmp", "/right.bmp" };
        for( const char* image : images_to_preload )
        {
            display::PreloadImage( image );
        }
    }
#if ENABLE_APPLE_NOTIFICATIONS
    NotificationProcessor::Init();
#endif

    {
        // write the splash screen to the display buffer before starting BLE. this uses a huge amount of RAM temporarily.
        display::DrawSplash( &gDisplay );
        gTft->DrawCanvas( &gDisplay );
        // gTft->DrawBMPDDirect( "/OldSols.bmp" );
    }

    Bluetooth::Begin( BluetoothDeviceName );
    // leave splash screen up for a few seconds.
    delay( 3000 );


#ifdef DEMO_MODE
    while( 1 )
    {
        Demo::Demo( &gDisplay, gTft );
        esp_task_wdt_reset();
    }
#endif

    // go back to sleep if the car is off, or go into the alarm mode if the lights are on.
    LOG_TRACE( "setup, HandlePowerState..." );
    HandlePowerState( CarIO::GetStatus() );

    // if the ignition is off, we wait until it's on, or we go to sleep.
    while( LightsAlarmActive )
    {
        HandlePowerState( CarIO::GetStatus() );
        CarIO::Service();
        esp_task_wdt_reset();
    }
    LOG_TRACE( "setup, GPS wake..." );
    Gps::Wake();

    LOG_TRACE( "setup, bluetooth service..." );
    Bluetooth::Service(); // service once to see if we're already connected!
    if( !Bluetooth::IsConnected() )
    {
        // if we're still not connected, display the connection message. If we still have correct time stored in the RTC system, show time
        // too.
        display::DiscoverableProps discoverable;
        discoverable.bluetoothName = BluetoothDeviceName.c_str();
        display::DrawDiscoverable( &gDisplay, discoverable );
        gTft->DrawCanvas( &gDisplay );
        delay( 1000 );
        // TODO: should we keep this screen up until we get the first connection? Especially since we don't know the time yet?
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
        LOG_TRACE( "Ignition and lights are off. Going to sleep." );
        Sleep();
        return;
    }

    // If IGN is gone but Illumination is ON, display lights alarm.
    if( !car_status.mIgnition && car_status.mLights )
    {
        if( !LightsAlarmActive )
        {
            LOG_TRACE( "Ignition is off, but Lights are on. Alarm" );
            LightsAlarmActive = true;
            CarIO::StartBeeper( 4, 1100, 80, 125, 1600 );
            display::DrawLightsAlarm( &gDisplay );
            gTft->DrawCanvas( &gDisplay );
        }
        return;
    }

    // If IGN is present, return to normal behavior.
    if( car_status.mIgnition )
    {
        if( LightsAlarmActive )
        {
            LOG_TRACE( "Ignition on, turning off the lights alarm." );
            LightsAlarmActive = false;
            CarIO::StopBeeper();
        }
    }
}

void Sleep()
{
    LOG_TRACE( "going to sleep..." );
    Gps::Sleep();
    gTft->EnableSleep( true );
    esp_sleep_enable_ext1_wakeup( ( 1ull << Pin::Ignition ) | ( 1ull << Pin::Illumination ), ESP_EXT1_WAKEUP_ANY_HIGH );
    Serial.flush();
    delay( 5 );
    esp_deep_sleep_start();
}

void HandleStatusUpdate( const CarIO::CarStatus& car_status )
{
    Bluetooth::SetVehicleStatus( car_status );
}


void loop()
{
    esp_task_wdt_reset();
    DebugService::HandleAssertCase();
    static uint32_t last_memory_update = millis();
    if( millis() - last_memory_update > 5000 )
    {
        last_memory_update = millis();
        PRINT_MEMORY_USAGE();
    }
    {
        auto motion_state = Motion::GetState();
        if( motion_state.mValid )
        {
            auto now = millis();
            BrakeHistoryTracker.PushData( motion_state.mForward, now );
            LateralHistoryTracker.PushData( motion_state.mLeft, now );
        }
    }
    uint32_t fw_bytes = 0;
    if( BleOta::IsInProgress( &fw_bytes ) )
    {
        if( !FwUpdateInProgress )
        {
            FwUpdateInProgress = true;
            // update the display
            LOG_TRACE( "FW Update started. Disabling normal functions." );
        }

        if( BleOta::IsRebootRequired() )
        {
            esp_restart();
            return;
        }

        if( fw_bytes >= FwBytesReceived )
        {
            FwBytesReceived = fw_bytes;
            display::OtaInProgressProps ota_screen;
            ota_screen.bytesReceived = fw_bytes;
            display::DrawOtaInProgress( &gDisplay, ota_screen );
            gTft->DrawCanvas( &gDisplay );
        }
        return; // don't do anything else while we're updating.
    }
    Bluetooth::Service();
    Gps::Service();
    CarIO::Service();

    // Live navigation matcher: when a route is loaded and the GPS has a fresh
    // fix, run the matcher (~1Hz on each new fix), store the result for the
    // Navigation screen, and log it (+ timing) for road testing.
    if( NavigationService::HasRoute() )
    {
        static uint32_t last_match_loc_age_ms = 0xFFFFFFFF;
        auto* gps = Gps::GetGps();
        gNavHasFix = gps->location.isValid() && gps->location.age() < 5000;
        if( gNavHasFix )
        {
            // Only re-run when the location has updated since last time (~1Hz).
            uint32_t age = gps->location.age();
            if( age < last_match_loc_age_ms )
            {
                last_match_loc_age_ms = age;
                nav::GpsFix fix{ gps->location.lat(), gps->location.lng() };
                const auto& route = NavigationService::GetRoute();
                auto t0 = micros();
                gNavResult = nav::match( route, fix );
                auto match_us = micros() - t0;
                gNavInstruction.clear();
                if( gNavResult.nextManeuverIndex >= 0 &&
                    gNavResult.nextManeuverIndex < ( int )route.maneuvers.size() )
                {
                    gNavInstruction = route.maneuvers[ gNavResult.nextManeuverIndex ].instruction;
                }
                LOG_INFO( "nav: along=%.0f off=%.1f%s toTurn=%.0f toDest=%.0f match=%uus next=\"%s\"",
                          gNavResult.distanceAlongRouteMeters, gNavResult.offRouteDistanceMeters,
                          gNavResult.isOffRoute ? " OFFROUTE" : "",
                          gNavResult.distanceToNextTurnMeters, gNavResult.distanceToDestinationMeters,
                          ( unsigned )match_us, gNavInstruction.c_str() );
            }
            else
            {
                last_match_loc_age_ms = age;
            }
        }
    }
    else
    {
        gNavHasFix = false;
    }
    auto car_status = CarIO::GetStatus();
    auto button_events = CarIO::GetButtonEvents();
    if( button_events.mHourButtonPressed )
    {
        LOG_TRACE( "hour button clicked" );
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
#if ENABLE_APPLE_NOTIFICATIONS
        else if( gCurrentScreen == CurrentScreen::QM )
        {
            gCurrentScreen = CurrentScreen::Notifications;
        }
        else if( gCurrentScreen == CurrentScreen::Notifications )
        {
            gCurrentScreen = CurrentScreen::Navigation;
        }
#else
        else if( gCurrentScreen == CurrentScreen::QM )
        {
            gCurrentScreen = CurrentScreen::Navigation;
        }
#endif
        else if( gCurrentScreen == CurrentScreen::Navigation )
        {
            gCurrentScreen = CurrentScreen::GMeter;
        }
        else if( gCurrentScreen == CurrentScreen::GMeter )
        {
            gCurrentScreen = CurrentScreen::Default;
        }
    }
    if( button_events.mMinuteButtonPressed )
    {
        LOG_TRACE( "minute button clicked" );
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
        display::ClockProps clock;
        tm time;
        if( getLocalTime( &time, 100 ) )
        {
            clock.hours24 = time.tm_hour;
            clock.minutes = time.tm_min;
        }

        if( Gps::GetGps()->speed.isValid() && Gps::GetGps()->speed.age() < 60000 )
        {
            clock.speedMph = Gps::GetGps()->speed.mph();
        }
        clock.headlight = car_status.mLights;
        clock.bluetooth = Bluetooth::IsConnected();
        const auto& media_info = AppleMediaService::GetMediaInformation();
        std::string media_title;
        if( media_info.mPlaybackState == AppleMediaService::MediaInformation::PlaybackState::Playing )
        {
            media_title = media_info.mTitle;
            clock.mediaTitle = media_title.c_str();
        }

        display::DrawClock( &gDisplay, clock );
        gTft->DrawCanvas( &gDisplay );
    }
    else if( gCurrentScreen == CurrentScreen::Status )
    {
        display::StatusProps status;
        status.batteryVolts = car_status.mBatteryVoltage;
        if( Gps::GetGps()->location.isValid() )
        {
            status.latitude = Gps::GetGps()->location.lat();
            status.longitude = Gps::GetGps()->location.lng();
            status.speedMph = Gps::GetGps()->speed.mph();
            status.headingDegrees = Gps::GetGps()->course.deg();
        }
        auto motion = Motion::GetState();
        if( motion.mValid )
        {
            status.forwardG = motion.mForward;
            status.lateralG = motion.mLeft;
            status.verticalG = motion.mUp;
        }
        display::DrawStatus( &gDisplay, status );
        gTft->DrawCanvas( &gDisplay );
    }
    else if( gCurrentScreen == CurrentScreen::QM )
    {
        QuarterMile::Service( &gDisplay, gTft, button_events, Gps::GetGps() );
    }
#if ENABLE_APPLE_NOTIFICATIONS
    else if( gCurrentScreen == CurrentScreen::Notifications )
    {
        AppleNotifications::NotificationSummary latest_notification;
        display::NotificationsProps notifications;
        notifications.hasNotification = NotificationProcessor::GetLatestNotification( latest_notification );
        std::string n_title = latest_notification.mTitle.value_or( "--" );
        std::string n_message = latest_notification.mMessage.value_or( "--" );
        std::string n_subtitle = latest_notification.mSubtitle.value_or( "--" );
        notifications.notification.title = n_title.c_str();
        notifications.notification.message = n_message.c_str();
        notifications.notification.subtitle = n_subtitle.c_str();
        notifications.notificationCount = NotificationProcessor::GetNotificationCount();
        display::DrawNotifications( &gDisplay, notifications );
        gTft->DrawCanvas( &gDisplay );
    }
#endif
    else if( gCurrentScreen == CurrentScreen::Navigation )
    {
        // Matcher-driven turn-by-turn screen (flag-independent). Fed by the live
        // matcher result computed earlier this loop.
        display::NavRouteProps nav_props;
        nav_props.hasRoute = NavigationService::HasRoute();
        nav_props.hasFix = gNavHasFix;
        nav_props.isOffRoute = gNavResult.isOffRoute;
        nav_props.instruction = gNavInstruction.c_str();
        nav_props.distanceToTurnMeters = gNavResult.distanceToNextTurnMeters;
        nav_props.offRouteDistanceMeters = gNavResult.offRouteDistanceMeters;
        display::DrawNavRoute( &gDisplay, nav_props );
        gTft->DrawCanvas( &gDisplay );
    }
    else if( gCurrentScreen == CurrentScreen::GMeter )
    {
        if( button_events.mMinuteButtonPressed )
        {
            Motion::Calibrate();
        }

        auto motion = Motion::GetState();
        if( !motion.mCalibrated )
        {
            display::DrawCalibrationMissing( &gDisplay );
            gTft->DrawCanvas( &gDisplay );
        }
        else
        {
            display::GMeterProps g_meter;
            double brake_history[ display::GMeterProps::HistorySize ] = { 0 };
            double lateral_history[ display::GMeterProps::HistorySize ] = { 0 };
            if( motion.mValid )
            {
                g_meter.brakeLive = motion.mForward;
                g_meter.lateralLive = motion.mLeft;

                BrakeHistoryTracker.GetData( brake_history, display::GMeterProps::HistorySize );
                LateralHistoryTracker.GetData( lateral_history, display::GMeterProps::HistorySize );
                g_meter.brakeHistory = brake_history;
                g_meter.lateralHistory = lateral_history;
            }
            display::DrawGMeter( &gDisplay, g_meter );
            gTft->DrawCanvas( &gDisplay );
        }
    }
    delay( 10 );
}
