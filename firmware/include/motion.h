#pragma once

#include <Adafruit_LSM6DS3.h>


namespace Motion
{
    struct State
    {
        float mForward; // X
        float mLeft; // Y
        float mUp; // Z
        bool mValid{ false };
    };


    void Begin();

    State GetState();

}