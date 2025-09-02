#include "apple_notification.h"

#include "logger.h"

namespace AppleNotifications
{
    namespace Detail
    {
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

        time_t ParseDateTime( const std::string& date_string )
        {
            // date string is in the format of "yyyyMMdd'T'HHmmSS"
            // example "20240409T200303"
            tm time;
            try
            {
                time.tm_year = std::stoi( date_string.substr( 0, 4 ) ) - 1900;
                time.tm_mon = std::stoi( date_string.substr( 4, 2 ) ) - 1;
                time.tm_mday = std::stoi( date_string.substr( 6, 2 ) );
                // T character.
                time.tm_hour = std::stoi( date_string.substr( 9, 2 ) );
                time.tm_min = std::stoi( date_string.substr( 11, 2 ) );
                time.tm_sec = std::stoi( date_string.substr( 13, 2 ) );
            }
            catch( const std::exception& e )
            {
                LOG_ERROR( "failed to parse DateTime %s. err: %s", date_string.c_str(), e.what() );
            }


            return mktime( &time );
        }

        bool IsNavigationNotification( const AppleNotifications::NotificationSummary& notification )
        {
            // hide "Traffic data received." messages
            if( notification.mAppIdentifier != "com.google.Maps" && notification.mAppIdentifier != "com.apple.Maps" )
            {
                return false;
            }
            if( notification.mMessage == "Traffic data received." )
            {
                return false;
            }
            return true;
        }
    }

    Notification Notification::Parse( uint8_t* data, int length )
    {
        Notification notification;
        notification.mEventId = static_cast<EventId>( data[ 0 ] );
        notification.mEventFlags = data[ 1 ];
        notification.mCategoryId = static_cast<CategoryId>( data[ 2 ] );
        notification.mCategoryCount = data[ 4 ];
        notification.mNotificationUID = Detail::Int32FromBytes( data + 4 );

        return notification;
    }

    void Notification::Dump() const
    {
        LOG_INFO( "event id: %hhx, flags: %hhx, category: %hhx, count: %hhx, notification Id: %lu", mEventId, mEventFlags, mCategoryId,
                  mCategoryCount, mNotificationUID );
    }

    NotificationAttribute NotificationAttribute::Parse( uint8_t* data, int length )
    {
        NotificationAttribute attribute;
        attribute.mCommandId = static_cast<CommandId>( data[ 0 ] );
        attribute.mNotificationUID = Detail::Int32FromBytes( data + 1 );
        attribute.mAttributeId = static_cast<AttributeId>( data[ 5 ] );
        uint16_t attn_length = data[ 6 ] | ( data[ 7 ] << 8 );
        if( attn_length > ( length - 8 ) )
        {
            LOG_ERROR( "Attribute length is longer than the data. att len: %u, remaining len: %u", attn_length, length - 8 );
            attn_length = length - 8;
        }
        attribute.mValue = std::string( reinterpret_cast<char*>( data + 8 ), attn_length );
        return attribute;
    }

    void NotificationAttribute::Dump()
    {
        LOG_INFO( "Command: %hhx, NotificationUID: %lu, Attribute: %hhx, Value: \"%s\" [%u]", mCommandId, mNotificationUID, mAttributeId,
                  mValue.c_str(), mValue.length() );
    }

    void NotificationSummary::SetAttribute( AttributeId id, const std::string& value )
    {
        switch( id )
        {
        case AttributeId::AppIdentifier:
            mAppIdentifier = value;
            break;
        case AttributeId::Title:
            mTitle = value;
            break;
        case AttributeId::Subtitle:
            mSubtitle = value;
            break;
        case AttributeId::Message:
            mMessage = value;
            break;
        case AttributeId::Date:
            mDateString = value;
            mDateTime = AppleNotifications::Detail::ParseDateTime( value );
            break;
        default:
            LOG_ERROR( "Unknown attribute ID: %hhx", id );
            break;
        }
    }

    void NotificationSummary::Dump()
    {
        LOG_INFO( "Notification Summary:" );
        LOG_INFO( "UID: %u", mNotificationUID );
        if( mAppIdentifier.has_value() )
        {
            LOG_INFO( "App Identifier: %s", mAppIdentifier->c_str() );
        }
        else
        {
            LOG_INFO( "App Identifier: <not requested>" );
        }
        if( mTitle.has_value() )
        {
            LOG_INFO( "Title: %s", mTitle->c_str() );
        }
        else
        {
            LOG_INFO( "Title: <not requested>" );
        }
        if( mSubtitle.has_value() )
        {
            LOG_INFO( "Subtitle: %s", mSubtitle->c_str() );
        }
        else
        {
            LOG_INFO( "Subtitle: <not requested>" );
        }
        if( mMessage.has_value() )
        {
            LOG_INFO( "Message: %s", mMessage->c_str() );
        }
        else
        {
            LOG_INFO( "Message: <not requested>" );
        }
        if( mDateString.has_value() )
        {
            LOG_INFO( "Date String: %s", mDateString->c_str() );
        }
        else
        {
            LOG_INFO( "Date String: <not requested>" );
        }
        if( mDateTime.has_value() )
        {
            char time_string[ 64 ];
            strftime( time_string, sizeof( time_string ), "%Y-%m-%d %H:%M:%S", localtime( &( mDateTime.value() ) ) );
            LOG_INFO( "Date Time: %s", time_string );
        }
    }

    std::optional<AttributeId> NotificationSummary::NextAttributeToRequest() const
    {
        if( !mDateString.has_value() )
        {
            return AttributeId::Date;
        }
        if( !mTitle.has_value() )
        {
            return AttributeId::Title;
        }
        if( !mSubtitle.has_value() )
        {
            return AttributeId::Subtitle;
        }
        if( !mMessage.has_value() )
        {
            return AttributeId::Message;
        }
        if( !mAppIdentifier.has_value() )
        {
            return AttributeId::AppIdentifier;
        }
        return std::nullopt;
    }

    bool NotificationSummary::IsComplete() const
    {
        return mAppIdentifier.has_value() && mTitle.has_value() && mSubtitle.has_value() && mMessage.has_value() && mDateString.has_value();
    }

}