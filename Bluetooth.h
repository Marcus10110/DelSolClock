#pragma once
#include <string>

namespace Bluetooth
{
    void Begin();
    void End();
    void Service();
    bool IsConnected();
    bool IsTimeSet();
    void SetVehicleStatus( const std::string& status );
}