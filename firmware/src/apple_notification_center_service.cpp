#include "apple_notification_center_service.h"
#include "logger.h"

#include <BLEClient.h>
#include <Arduino.h>

#include <map>
#include <mutex>

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
                LOG_INFO( "event id: %hhx, flags: %hhx, category: %hhx, count: %hhx, notification Id: %lu", mEventId, mEventFlags,
                          mCategoryId, mCategoryCount, mNotificationUID );
            }
        };


        enum class CommandId : uint8_t
        {
            GetNotificationAttributes = 0,
            GetAppAttributes = 1,
            PerformNotificationAction = 2
        };

        enum class AttributeId : uint8_t
        {
            AppIdentifier = 0,
            Title = 1,    // (Needs to be followed by a 2-bytes max length parameter)
            Subtitle = 2, // (Needs to be followed by a 2-bytes max length parameter)
            Message = 3,  // (Needs to be followed by a 2-bytes max length parameter)
            MessageSize = 4,
            Date = 5,
            PositiveActionLabel = 6,
            NegativeActionLabel = 7
        };

        struct NotificationAttribute
        {
            CommandId mCommandId;
            uint32_t mNotificationUID;
            AttributeId mAttributeId;
            // Note, this only supports 1 attribute at a time.
            String mValue;

            static NotificationAttribute Parse( uint8_t* data, int length )
            {
                NotificationAttribute attribute;
                attribute.mCommandId = static_cast<CommandId>( data[ 0 ] );
                attribute.mNotificationUID = Int32FromBytes( data + 1 );
                attribute.mAttributeId = static_cast<AttributeId>( data[ 5 ] );
                uint16_t attn_length = data[ 6 ] | ( data[ 7 ] << 8 );
                if( attn_length > ( length - 8 ) )
                {
                    LOG_ERROR( "Attribute length is longer than the data. att len: %u, remaining len: %u", attn_length, length - 8 );
                    attn_length = length - 8;
                }
                attribute.mValue = String( reinterpret_cast<char*>( data + 8 ), attn_length );
                return attribute;
            }

            void Dump()
            {
                LOG_INFO( "Command: %hhx, NotificationUID: %lu, Attribute: %hhx, Value: \"%s\" [%u]", mCommandId, mNotificationUID,
                          mAttributeId, mValue.c_str(), mValue.length() );
            }
        };


        struct NotificationSummary
        {
            Notification mNotification;
            bool mDetailRequested{ false };
            uint32_t mReceivedTime{ 0 };
            String mAppIdentifier;
            String mTitle;
            String mSubtitle;
            String mMessage;
            String mDateString;
            time_t mDateTime;

            void Dump()
            {
                LOG_INFO( "Notification Summary:" );
                mNotification.Dump();
                // LOG_INFO( "Detail Requested: %d", mDetailRequested );
                // LOG_INFO( "Received Time: %lu", mReceivedTime );
                LOG_INFO( "App Identifier: %s", mAppIdentifier.c_str() );
                LOG_INFO( "Title: %s", mTitle.c_str() );
                LOG_INFO( "Subtitle: %s", mSubtitle.c_str() );
                LOG_INFO( "Message: %s", mMessage.c_str() );
                LOG_INFO( "Date String: %s", mDateString.c_str() );
                // LOG_INFO( "Date Time: %lu", mDateTime );
            }
        };

        std::map<uint32_t, NotificationSummary> OpenNotifications;

        BLERemoteCharacteristic* ControlPoint = nullptr;

        std::mutex mMutex;

        void RequestAttributesForNotification( uint32_t notification_uid )
        {
            if( !ControlPoint )
            {
                LOG_ERROR( "ControlPoint not initialized" );
                return;
            }
            LOG_TRACE( "Requesting attributes for notification %lu", notification_uid );
            uint8_t uid[ 4 ];
            uid[ 0 ] = notification_uid;
            uid[ 1 ] = notification_uid >> 8;
            uid[ 2 ] = notification_uid >> 16;
            uid[ 3 ] = notification_uid >> 24;
            uint8_t app_identifier_command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ),
                                                 uid[ 0 ],
                                                 uid[ 1 ],
                                                 uid[ 2 ],
                                                 uid[ 3 ],
                                                 static_cast<uint8_t>( AttributeId::AppIdentifier ) };
            ControlPoint->writeValue( app_identifier_command, sizeof( app_identifier_command ), true );
            uint8_t title_command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ),
                                        uid[ 0 ],
                                        uid[ 1 ],
                                        uid[ 2 ],
                                        uid[ 3 ],
                                        static_cast<uint8_t>( AttributeId::Title ),
                                        0x0,
                                        0x10 };
            ControlPoint->writeValue( title_command, sizeof( title_command ), true );
            uint8_t subtitle_command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ),
                                           uid[ 0 ],
                                           uid[ 1 ],
                                           uid[ 2 ],
                                           uid[ 3 ],
                                           static_cast<uint8_t>( AttributeId::Subtitle ),
                                           0x0,
                                           0x10 };
            ControlPoint->writeValue( subtitle_command, sizeof( subtitle_command ), true );
            uint8_t message_command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ),
                                          uid[ 0 ],
                                          uid[ 1 ],
                                          uid[ 2 ],
                                          uid[ 3 ],
                                          static_cast<uint8_t>( AttributeId::Message ),
                                          0x0,
                                          0x10 };
            ControlPoint->writeValue( message_command, sizeof( message_command ), true );
            uint8_t date_command[] = { static_cast<uint8_t>( CommandId::GetNotificationAttributes ),
                                       uid[ 0 ],
                                       uid[ 1 ],
                                       uid[ 2 ],
                                       uid[ 3 ],
                                       static_cast<uint8_t>( AttributeId::Date ) };
            ControlPoint->writeValue( date_command, sizeof( date_command ), true );
        }

        time_t ParseDateTime( const String& date_string )
        {
            // date string is in the format of "yyyyMMdd'T'HHmmSS"
            // example "20240409T200303"
            tm time;
            time.tm_year = date_string.substring( 0, 4 ).toInt() - 1900;
            time.tm_mon = date_string.substring( 4, 6 ).toInt() - 1;
            time.tm_mday = date_string.substring( 6, 8 ).toInt();
            // T character.
            time.tm_hour = date_string.substring( 9, 11 ).toInt();
            time.tm_min = date_string.substring( 11, 13 ).toInt();
            time.tm_sec = date_string.substring( 13, 15 ).toInt();

            return mktime( &time );
        }


    }
    bool StartNotificationService( BLEClient* client )
    {
        assert( client != nullptr );

        if( !client->isConnected() )
        {
            LOG_ERROR( "client not connected" );
            return false;
        }

        auto notification_service = client->getService( ANCS_SERVICE_UUID );
        if( !notification_service )
        {
            LOG_ERROR( "Apple ANCS service not found" );
            return false;
        }

        auto notification_source = notification_service->getCharacteristic( APPLE_NOTIFICATION_SOURCE_CHARACTERISTIC_UUID );
        if( !notification_source )
        {
            LOG_ERROR( "Apple notification source characteristic not found" );
            return false;
        }

        notification_source->registerForNotify(
            []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
                std::scoped_lock lock( mMutex );
                if( length != 8 )
                {
                    LOG_ERROR( "Apple notification source message not exactly 8 bytes" );
                    return;
                }
                LOG_INFO( "free memory: %d", ESP.getFreeHeap() );

                auto notification = Notification::Parse( data, length );
                notification.Dump();

                // if the notification is new, add it to the map.
                if( notification.mEventId == Notification::EventId::NotificationAdded )
                {
                    // check if the uid is already in use:
                    if( OpenNotifications.find( notification.mNotificationUID ) != OpenNotifications.end() )
                    {
                        LOG_ERROR( "Notification already exists" );
                        return;
                    }
                    OpenNotifications[ notification.mNotificationUID ].mNotification = notification;
                    OpenNotifications[ notification.mNotificationUID ].mReceivedTime = millis();
                    OpenNotifications[ notification.mNotificationUID ].mDetailRequested = false;
                }
                else if( notification.mEventId == Notification::EventId::NotificationRemoved )
                {
                    OpenNotifications.erase( notification.mNotificationUID );
                }
                else if( notification.mEventId == Notification::EventId::NotificationModified )
                {
                    if( OpenNotifications.count( notification.mNotificationUID ) == 0 )
                    {
                        LOG_ERROR( "Received modified notification for unknown notification" );
                        return;
                    }
                    auto& notification_summary = OpenNotifications[ notification.mNotificationUID ];
                    notification_summary.mReceivedTime = millis();
                    notification_summary.mDetailRequested = false;
                }
            } );

        // setup control point
        ControlPoint = notification_service->getCharacteristic( controlPointCharacteristicUUID );
        if( !ControlPoint )
        {
            LOG_ERROR( "Apple control point characteristic not found" );
            return false;
        }

        // setup data source
        auto data_source = notification_service->getCharacteristic( dataSourceCharacteristicUUID );
        if( !data_source )
        {
            LOG_ERROR( "Apple data source characteristic not found" );
            return false;
        }
        // setup callback for data source notifications
        data_source->registerForNotify( []( BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool is_notify ) {
            LOG_TRACE( "Received data from data source. length: %u", length );
            LOG_INFO( "free memory: %d", ESP.getFreeHeap() );
            std::scoped_lock lock( mMutex );
            if( length < 5 )
            {
                LOG_ERROR( "Apple data source message not at least 5 bytes" );
                return;
            }
            auto notification_attribute = NotificationAttribute::Parse( data, length );
            notification_attribute.Dump();

            auto summary = OpenNotifications.find( notification_attribute.mNotificationUID );
            if( summary == OpenNotifications.end() )
            {
                LOG_ERROR( "Received data for unknown notification" );
                return;
            }

            if( summary->second.mDetailRequested )
            {
                // parse the data
                switch( notification_attribute.mAttributeId )
                {
                case AttributeId::AppIdentifier:
                    summary->second.mAppIdentifier = notification_attribute.mValue;
                    break;
                case AttributeId::Title:
                    summary->second.mTitle = notification_attribute.mValue;
                    break;
                case AttributeId::Subtitle:
                    summary->second.mSubtitle = notification_attribute.mValue;
                    break;
                case AttributeId::Message:
                    summary->second.mMessage = notification_attribute.mValue;
                    break;
                case AttributeId::Date:
                    summary->second.mDateString = notification_attribute.mValue;
                    summary->second.mDateTime = ParseDateTime( notification_attribute.mValue );
                    summary->second.Dump();
                    break;
                default:
                    LOG_ERROR( "Unknown attribute id: %u", notification_attribute.mAttributeId );
                    break;
                }
            }
        } );

        return true;
    }

    void Service()
    {
        // LOG_TRACE( "AppleNotifications::Service" );
        auto now = millis();
        constexpr uint32_t minimum_age_ms = 3000;
        if( now < minimum_age_ms )
        {
            return;
        }
        std::unique_lock lock( mMutex );
        // go through all notifications and request details where needed.
        for( auto& notification : OpenNotifications )
        {
            if( notification.second.mDetailRequested )
            {
                continue;
            }
            if( now - notification.second.mReceivedTime < minimum_age_ms )
            {
                continue;
            }
            notification.second.mDetailRequested = true;
            lock.unlock();
            RequestAttributesForNotification( notification.first );
            // LOG_TRACE( "AppleNotifications::Service returning" );
            return; // only request 1 at a time?
        }
    }

    bool GetLatestNotification( DisplayNotification& notification )
    {
        std::scoped_lock lock( mMutex );
        if( OpenNotifications.empty() )
        {
            return false;
        }
        auto latest = OpenNotifications.begin()->second;
        for( auto& n : OpenNotifications )
        {
            if( n.second.mDateTime > latest.mDateTime )
            {
                latest = n.second;
            }
        }
        notification.mTitle = latest.mTitle.c_str();
        notification.mSubtitle = latest.mSubtitle.c_str();
        notification.mMessage = latest.mMessage.c_str();
        notification.mAppIdentifier = latest.mAppIdentifier.c_str();
        return true;
    }

    bool GetLatestNavigationNotification( DisplayNotification& notification )
    {
        std::scoped_lock lock( mMutex );
        if( OpenNotifications.empty() )
        {
            return false;
        }
        // find the latest notification with the app identification of "com.google.Maps" or "com.apple.Maps" based on mReceivedTime.
        std::optional<uint32_t> most_recent_nav_notification_uid;
        for( auto& n : OpenNotifications )
        {
            if( n.second.mAppIdentifier == "com.google.Maps" || n.second.mAppIdentifier == "com.apple.Maps" )
            {
                if( !most_recent_nav_notification_uid.has_value() ||
                    n.second.mReceivedTime > OpenNotifications[ most_recent_nav_notification_uid.value() ].mReceivedTime )
                {
                    most_recent_nav_notification_uid = n.first;
                }
            }
        }
        if( !most_recent_nav_notification_uid.has_value() )
        {
            return false; // no navigation notification found.
        }
        auto& latest = OpenNotifications[ most_recent_nav_notification_uid.value() ];
        notification.mTitle = latest.mTitle.c_str();
        notification.mSubtitle = latest.mSubtitle.c_str();
        notification.mMessage = latest.mMessage.c_str();
        notification.mAppIdentifier = latest.mAppIdentifier.c_str();
        LOG_INFO( "Latest navigation notification: %s, %s, %s, %s", notification.mTitle.c_str(), notification.mSubtitle.c_str(),
                  notification.mMessage.c_str(), notification.mAppIdentifier.c_str() );
        return true;
    }
}