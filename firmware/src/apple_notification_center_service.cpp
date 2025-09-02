#include "apple_notification_center_service.h"
#include "logger.h"

#include "apple_notification.h"
#include "notification_processor.h"

#include "utilities.h"

#include <BLEClient.h>
#include <Arduino.h>

#include <map>
#include <mutex>
#include <queue>
#include <atomic>

#include <esp32-hal-log.h>
#include <esp_task_wdt.h>
/*
Code mostly taken from here and refactored to work with an existing client connection:
https://github.com/Smartphone-Companions/ESP32-ANCS-Notifications/blob/master/src/ancs_ble_client.cpp
*/

namespace AppleNotifications
{
    namespace
    {
        // Fixed service IDs for the Apple ANCS service
        const BLEUUID APPLE_NOTIFICATION_SOURCE_CHARACTERISTIC_UUID( "9FBF120D-6301-42D9-8C58-25E699A21DBD" );
        const BLEUUID controlPointCharacteristicUUID( "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9" );
        const BLEUUID dataSourceCharacteristicUUID( "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB" );
        const BLEUUID ANCS_SERVICE_UUID( "7905F431-B5CE-4E99-A40F-4B1E122D00D0" );

        BLERemoteCharacteristic* ControlPoint = nullptr;

        void RequestAttribute( uint32_t uuid, AppleNotifications::AttributeId attribute_id )
        {
            if( !ControlPoint )
            {
                LOG_ERROR( "ControlPoint not initialized" );
                return;
            }

            uint8_t uid[ 4 ];
            uid[ 0 ] = uuid;
            uid[ 1 ] = uuid >> 8;
            uid[ 2 ] = uuid >> 16;
            uid[ 3 ] = uuid >> 24;

            bool includes_max_length =
                ( attribute_id == AppleNotifications::AttributeId::Title || attribute_id == AppleNotifications::AttributeId::Subtitle ||
                  attribute_id == AppleNotifications::AttributeId::Message );

            uint8_t command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ), uid[ 0 ], uid[ 1 ], uid[ 2 ], uid[ 3 ],
                                  static_cast<uint8_t>( attribute_id ),
                                  // these last two bytes are only used for title, subtitle, and message
                                  0x0, 0x10 };
            int command_length = includes_max_length ? 8 : 6;

            ControlPoint->writeValue( command, command_length, true );
        }

    }


    bool StartNotificationService( BLEClient* client )
    {
        assert( client != nullptr );

        LOG_TRACE( "Starting Apple Notification Service" );
        PRINT_MEMORY_USAGE();


        if( !client->isConnected() )
        {
            LOG_ERROR( "client not connected" );
            return false;
        }

        PRINT_MEMORY_USAGE();
        auto notification_service = client->getService( ANCS_SERVICE_UUID );
        if( !notification_service )
        {
            LOG_ERROR( "Apple ANCS service not found" );
            return false;
        }
        esp_task_wdt_reset();
        PRINT_MEMORY_USAGE();
        auto notification_source = notification_service->getCharacteristic( APPLE_NOTIFICATION_SOURCE_CHARACTERISTIC_UUID );
        if( !notification_source )
        {
            LOG_ERROR( "Apple notification source characteristic not found" );
            return false;
        }

        notification_source->registerForNotify(
            []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
                if( length != 8 )
                {
                    LOG_ERROR( "Apple notification source message not exactly 8 bytes" );
                    return;
                }
                auto notification = Notification::Parse( data, length );
                LOG_TRACE( "callback for new notification %u", notification.mNotificationUID );
                NotificationProcessor::HandleNotification( notification );
            } );

        // setup control point
        ControlPoint = notification_service->getCharacteristic( controlPointCharacteristicUUID );
        if( !ControlPoint )
        {
            LOG_ERROR( "Apple control point characteristic not found" );
            return false;
        }

        esp_task_wdt_reset();
        // setup data source
        auto data_source = notification_service->getCharacteristic( dataSourceCharacteristicUUID );
        if( !data_source )
        {
            LOG_ERROR( "Apple data source characteristic not found" );
            return false;
        }
        // setup callback for data source notifications
        data_source->registerForNotify( []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
            LOG_TRACE( "callback for data source notification" );
            NotificationProcessor::RawAttribute attribute;
            attribute.mLength = length;
            if( length > sizeof( attribute.mData ) )
            {
                LOG_ERROR( "data source notification too long: %d", length );
                return;
            }
            memcpy( attribute.mData, data, length );
            NotificationProcessor::HandleAttribute( attribute );
        } );

        const uint8_t v[] = { 0x1, 0x0 };
        data_source->getDescriptor( BLEUUID( ( uint16_t )0x2902 ) )->writeValue( ( uint8_t* )v, 2, true );
        notification_source->getDescriptor( BLEUUID( ( uint16_t )0x2902 ) )->writeValue( ( uint8_t* )v, 2, true );

        esp_task_wdt_reset();
        NotificationProcessor::Begin( RequestAttribute );
        LOG_TRACE( "Apple Notification Service started" );
        PRINT_MEMORY_USAGE();
        return true;
    }
}