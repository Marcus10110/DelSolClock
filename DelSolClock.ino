#include "AppleMediaService.h"
#include "CurrentTimeService.h"
#include "AppleNotificationCenterService.h"
#include "Display.h"
#include "pins.h"
#include "CarIO.h"
#include "Bluetooth.h"
#include <time.h>
#include <sys/time.h>
#include "driver/uart.h"
#include <TinyGPSPlus.h>

namespace
{
    bool TrackNameDirty = false;
    bool NeverConnected = true;
    constexpr uart_port_t GpsUartPort = UART_NUM_2;
    QueueHandle_t GpsUartQueue;
    TinyGPSPlus Gps;
}

void setup()
{
    CarIO::Setup();

    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );

    SetupGps();

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
}

void loop()
{
    Bluetooth::Service();
    ServiceGps();
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
    if( Gps.speed.isUpdated() )
    {
        new_state.mSpead = Gps.speed.mph();
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


void SetupGps()
{
    uart_config_t uart_config;
    uart_config.baud_rate = 9600;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 120;

    uart_param_config( GpsUartPort, &uart_config );

    uart_set_pin( GpsUartPort,
                  Pin::GpsRx,         // TX
                  Pin::GpsTx,         // RX
                  UART_PIN_NO_CHANGE, // RTS
                  UART_PIN_NO_CHANGE  // CTS
    );

    uart_driver_install( GpsUartPort, 2048, 2048, 10, &GpsUartQueue, 0 );
}

void ServiceGps()
{
    size_t count = 0;
    uint8_t data[ 128 ];
    uart_get_buffered_data_len( GpsUartPort, &count );
    if( count > 0 )
    {
        while( count > 0 )
        {
            auto read = uart_read_bytes( GpsUartPort, data, min( count, sizeof( data ) ), 100 );
            for( int i = 0; i < read; ++i )
            {
                Gps.encode( static_cast<char>( data[ i ] ) );
            }
            count -= read;
        }
    }

    if( Gps.location.isUpdated() )
    {
        float lat = Gps.location.lat();
        float lng = Gps.location.lng();
        Serial.printf( "GPS: %f, %f\n", lat, lng );
    }
}