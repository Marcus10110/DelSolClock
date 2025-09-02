#pragma once
#include <Arduino.h>

class BLEClient;

namespace AppleNotifications
{
    bool StartNotificationService( BLEClient* client );
}