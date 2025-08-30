#pragma once
#include <cstdint>

class BLEServer;

namespace BleOta
{
    void Begin( BLEServer* server );
    bool IsInProgress( uint32_t* bytes_received );
    bool IsRebootRequired();
}