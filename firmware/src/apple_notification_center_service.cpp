#include "apple_notification_center_service.h"

#include <BLEClient.h>
#include <Arduino.h>

#include <esp32-hal-log.h>
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

        // Little Endian Helper
        uint32_t Int32FromBytes( uint8_t* bytes )
        {
            uint32_t v = 0;
            v |= bytes[ 0 ];
            v |= static_cast<uint32_t>( bytes[ 1 ] ) << 8;
            v |= static_cast<uint32_t>( bytes[ 2 ] ) << 16;
            v |= static_cast<uint32_t>( bytes[ 3 ] ) << 24;
            return v;
        }

        struct Notification
        {
            enum class EventId : uint8_t
            {
                NotificationAdded = 0,
                NotificationModified = 1,
                NotificationRemoved = 2
            };
            enum class EventFlags : uint8_t
            {
                Sent = 1 << 0,
                Important = 1 << 1,
                PreExisting = 1 << 2,
                PositiveAction = 1 << 3,
                NegativeAction = 1 << 4
            };
            enum class CategoryId : uint8_t
            {
                Other = 0,
                IncomingCall = 1,
                MissedCall = 2,
                VoiceMail = 3,
                Social = 4,
                Schedule = 5,
                Email = 6,
                News = 7,
                HealthAndFitness = 8,
                BusinessAndFinance = 9,
                Location = 10,
                Entertainment = 11
            };

            EventId mEventId;
            uint8_t mEventFlags;
            CategoryId mCategoryId;
            uint8_t mCategoryCount;
            uint32_t mNotificationUID;

            static Notification Parse( uint8_t* data, int length )
            {
                Notification notification;
                notification.mEventId = static_cast<EventId>( data[ 0 ] );
                notification.mEventFlags = data[ 1 ];
                notification.mCategoryId = static_cast<CategoryId>( data[ 2 ] );
                notification.mCategoryCount = data[ 4 ];
                notification.mNotificationUID = Int32FromBytes( data + 4 );

                return notification;
            }

            void Dump() const
            {
                Serial.printf( "event id: %hhx, flags: %hhx, category: %hhx, count: %hhx, notification Id: %lu\n", mEventId, mEventFlags,
                               mCategoryId, mCategoryCount, mNotificationUID );
            }
        };
    }
    bool StartNotificationService( BLEClient* client )
    {
        assert( client != nullptr );

        if( !client->isConnected() )
        {
            log_e( "client not connected" );
            return false;
        }

        auto notification_service = client->getService( ANCS_SERVICE_UUID );
        if( !notification_service )
        {
            log_e( "Apple ANCS service not found" );
            return false;
        }

        auto notification_source = notification_service->getCharacteristic( APPLE_NOTIFICATION_SOURCE_CHARACTERISTIC_UUID );
        if( !notification_source )
        {
            log_e( "Apple notification source characteristic not found" );
            return false;
        }

        notification_source->registerForNotify(
            []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
                if( length != 8 )
                {
                    log_e( "Apple notification source message not exactly 8 bytes" );
                    return;
                }

                auto notification = Notification::Parse( data, length );
                notification.Dump();
            } );

        return true;
    }
}