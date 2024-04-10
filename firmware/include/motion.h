#pragma once

#include <Adafruit_LSM6DS3.h>


namespace Motion
{
    struct State
    {
        float mForward{ 0 }; // X
        float mLeft{ 0 };    // Y
        float mUp{ 0 };      // Z
        bool mValid{ false };
    };


    void Begin();

    State GetState();

}