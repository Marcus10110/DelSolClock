#pragma once
#include <string>
#include "car_io.h"

namespace Bluetooth
{
    void Begin( const std::string& device_name );
    void End();
    void Service();
    bool IsConnected();
    bool IsTimeSet();
    void SetVehicleStatus( const CarIO::CarStatus& car_status );
}