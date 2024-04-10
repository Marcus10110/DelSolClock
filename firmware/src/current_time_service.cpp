#include "current_time_service.h"
#include "logger.h"

#include <BLEClient.h>
#include <Arduino.h>
#include <time.h>

#include <esp32-hal-log.h>

// Current Time Service
// Documentation: https://www.bluetooth.com/specifications/specs/current-time-service-1-1/

// characteristic data layout defined in GATT Specification Supplement
// https://www.bluetooth.org/DocMan/handlers/DownloadDoc.ashx?doc_id=514633

static const BLEUUID CURRENT_TIME_SERVICE_UUID( ( uint16_t )0x1805 );

static const BLEUUID CURRENT_TIME_UUID( ( uint16_t )0x2A2B );           // mandatory
static const BLEUUID LOCAL_TIME_INFORMATION_UUID( ( uint16_t )0x2A0F ); // optional
static const BLEUUID REFERENCE_TIME_INFORMATION( ( uint16_t )0x2A14 );  // optional
namespace CurrentTimeService
{
    namespace
    {
        BLERemoteCharacteristic* gCurrentTimeCharacteristic = nullptr;


        const char* DayOfWeekToString( CurrentTime::DayOfWeek day )
        {
            switch( day )
            {
            case CurrentTime::DayOfWeek::Unknown:
                return "Unknown";
            case CurrentTime::DayOfWeek::Monday:
                return "Monday";
            case CurrentTime::DayOfWeek::Tuesday:
                return "Tuesday";
            case CurrentTime::DayOfWeek::Wednesday:
                return "Wednesday";
            case CurrentTime::DayOfWeek::Thursday:
                return "Thursday";
            case CurrentTime::DayOfWeek::Friday:
                return "Friday";
            case CurrentTime::DayOfWeek::Saturday:
                return "Saturday";
            case CurrentTime::DayOfWeek::Sunday:
                return "Sunday";
            }
            return "Unknown";
        }

        CurrentTime ParseCurrentTime( const uint8_t* data, size_t length )
        {
            CurrentTime time;
            if( length < 9 )
            {
                log_e( "time data length too short" );
                return time;
            }
            time.mYear = data[ 0 ] | ( data[ 1 ] << 8 );
            time.mMonth = data[ 2 ];
            time.mDay = data[ 3 ];
            time.mHours = data[ 4 ];
            time.mMinutes = data[ 5 ];
            time.mSeconds = data[ 6 ];
            // int weekday = data[7];
            time.mSecondsFraction = static_cast<float>( data[ 8 ] ) / 256.0;
            return time;
        }
    }

    void CurrentTime::Dump() const
    {
        if( mDayOfWeek != DayOfWeek::Unknown )
        {
            LOG_INFO( "%s, %hhu/%hhu/%hu %hhu:%hhu:%hhu.%f", DayOfWeekToString( mDayOfWeek ), mMonth, mDay, mYear, mHours, mMinutes,
                      mSeconds, mSecondsFraction );
        }
        else
        {
            LOG_INFO( "%hhu/%hhu/%hu %hhu:%hhu:%hhu.%f", mMonth, mDay, mYear, mHours, mMinutes, mSeconds, mSecondsFraction );
        }
    }
    time_t CurrentTime::ToTimeT() const
    {
        tm time;
        time.tm_year = mYear - 1900; // years since 1900
        time.tm_mon = mMonth - 1;    // months since January
        time.tm_mday = mDay;
        time.tm_hour = mHours;
        time.tm_min = mMinutes;
        time.tm_sec = mSeconds;
        time.tm_isdst = -1; // negative values mean unknown.

        return mktime( &time );
    }

    bool GetCurrentTime( CurrentTime* current_time )
    {
        if( !gCurrentTimeCharacteristic )
        {
            return false;
        }
        if( current_time )
        {
            auto data = gCurrentTimeCharacteristic->readValue();
            auto time = ParseCurrentTime( reinterpret_cast<const uint8_t*>( data.data() ), data.size() );
            *current_time = time;
            return true;
        }
        return false;
    }

    bool StartTimeService( BLEClient* client, CurrentTime* current_time )
    {
        assert( client != nullptr );

        if( !client->isConnected() )
        {
            log_e( "client not connected" );
            return false;
        }

        auto time_service = client->getService( CURRENT_TIME_SERVICE_UUID );
        if( !time_service )
        {
            log_e( "time service not found" );
            return false;
        }

        auto current_time_characteristic = time_service->getCharacteristic( CURRENT_TIME_UUID );
        if( !current_time_characteristic )
        {
            log_e( "current time characteristic not found" );
            return false;
        }

        // Current time only notifies when the time is manually changed, e.g. user edits the time, or the time zone is changed either
        // manually or automatically.
        current_time_characteristic->registerForNotify(
            []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
                // TODO: notify the application, if interested.
                LOG_INFO( "Current time has been edited." );
                auto time = ParseCurrentTime( data, length );
                time.Dump();
            } );

        // TODO: add support for the local time characteristic
        auto local_time_characteristic = time_service->getCharacteristic( LOCAL_TIME_INFORMATION_UUID );
        if( !local_time_characteristic )
        {
            LOG_WARN( "optional local time characteristic not found" );
        }

        // TODO: add support for the reference time characteristic
        auto reference_time_characteristic = time_service->getCharacteristic( REFERENCE_TIME_INFORMATION );
        if( !reference_time_characteristic )
        {
            LOG_WARN( "optional reference time characteristic not found" );
        }

        if( current_time != nullptr )
        {
            auto data = current_time_characteristic->readValue();
            auto time = ParseCurrentTime( reinterpret_cast<const uint8_t*>( data.data() ), data.size() );
            *current_time = time;
        }

        gCurrentTimeCharacteristic = current_time_characteristic;
        return true;
    }

    void StopTimeService()
    {
        // Note: when the client is deleted, it will delete all services and characteristics, etc.
        // This function just protects GetCurrentTime from an access violation.
        gCurrentTimeCharacteristic = nullptr;
    }
}