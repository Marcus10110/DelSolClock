#pragma once
#include <Arduino.h>

namespace Display
{
    void Begin();
    void Clear();
    void DrawTime( uint8_t hours24, uint8_t minutes );
}