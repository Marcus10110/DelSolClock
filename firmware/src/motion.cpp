#include "motion.h"

namespace Motion
{
    namespace
    {
        Adafruit_LSM6DS3 Sensor;
    }


    void Begin()
    {
        if( !Sensor.begin_I2C() )
        {
            while( true )
            {
                Serial.println( "failed to start I2C motion sensor" );
                delay( 1000 );
            }

            return;
        }
        Serial.println( "I2C motion sensor found!" );
    }
}