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

    State GetState()
    {
        State state;
        float x, y, z = 0;
        // If the clock is mounted vertically on the dash, the native coordinate system is as follows:
        // +x = left
        // +y = up
        // +z = forward
        if( Sensor.readAcceleration( x, y, z ) != 1 )
        {
            return state;
        }
        state.mForward = z;
        state.mLeft = x;
        state.mUp = y;


        // the clock is slightly tilted, so we'll need to adjust the forward vector.
        // it's rotated about Y, so X is pointing down about 15 degrees..

        // TODO: we'll need to rotate the forward vector about the X axis by 15 degrees.

        return state;
    }

}