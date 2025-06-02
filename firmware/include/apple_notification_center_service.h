#pragma once
#include <Arduino.h>

class BLEClient;

namespace AppleNotifications
{
    struct DisplayNotification
    {
        String mTitle;
        String mSubtitle;
        String mMessage;
        String mAppIdentifier;
    };


    bool StartNotificationService( BLEClient* client );
    void Service();
    bool GetLatestNotification( DisplayNotification& notification );
    bool GetLatestNavigationNotification( DisplayNotification& notification );
}