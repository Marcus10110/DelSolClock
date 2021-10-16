#pragma once
#include <cstdint>


#define REV2
namespace Pin
{
    // TFT & SD Card
    constexpr int8_t TftReset = 15;
    constexpr int8_t TftCs = 14;
    constexpr int8_t TftDc = 32;
    constexpr int8_t TftLit = 27;
    constexpr int8_t SdCs = 33;
    // SPI
    constexpr int8_t Sck = 5;
    constexpr int8_t Miso = 19;
    constexpr int8_t Mosi = 18;
    // Car Pins
    constexpr int8_t Battery = 4;
    constexpr int8_t Illumination = 36;
    constexpr int8_t Ignition = 39;
    constexpr int8_t RearWindow = 34;
    constexpr int8_t Trunk = 25;
    constexpr int8_t Roof = 26;
    // Clock Pins
    constexpr int8_t Hour = 13;
    constexpr int8_t Minute = 35; //  the Feather connects pin 35 to VBAT divider.
    // Other
    constexpr int8_t Buzzer = 12; // internal boot-related pull down. Use as output only.
    // GPS
#ifdef REV2
    constexpr int8_t GpsRx = 16;
    constexpr int8_t GpsTx = 17;
#elif
    // (note, these are the default I2C pins)
    constexpr int8_t GpsRx = 22;
    constexpr int8_t GpsTx = 23;
#endif

}