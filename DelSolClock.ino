// #define SIMULATION 1

#ifdef SIMULATION
// Simulation mode is used to allow simualting the UI with wokwi.
// wokwi seems to only work with the generic esp32 board target.
// The del sol clock hardware uses the Adafruit ESP32 Feather target.
#include "simulation_main.h"
namespace DelSolClock = DelSolClockSimulator;
#else
#include "main.h"
#endif


void setup()
{
    DelSolClock::Setup();
}

void loop()
{
    DelSolClock::Loop();
}