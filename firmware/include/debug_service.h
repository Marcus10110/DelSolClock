#pragma once

class BLEServer;
// The debug service allows connected BLE devices to check for and download ESP32 crash dumps from flash.
namespace DebugService
{
    bool StartDebugService( BLEServer* server );
}