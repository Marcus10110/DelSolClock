#pragma once
#include "apple_notification.h"

#include <functional>

namespace NotificationProcessor
{
    struct RawAttribute
    {
        int mLength;
        uint8_t mData[ 512 ];
    };

    typedef void ( *RequestAttributeCallback )( uint32_t uuid, AppleNotifications::AttributeId attribute_id );

    void Init();
    void Begin( RequestAttributeCallback attribute_request_callback );
    void HandleNotification( const AppleNotifications::Notification& notification );
    void HandleAttribute( const RawAttribute& attribute );

    bool GetLatestNotification( AppleNotifications::NotificationSummary& notification );
    bool GetLatestNavigationNotification( AppleNotifications::NotificationSummary& notification );
}
