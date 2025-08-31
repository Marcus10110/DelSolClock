#include "motion.h"
#include "logger.h"
#include <cassert>
// #include <EEPROM.h>
#include "vec.h"

namespace Motion
{
    namespace
    {
        Adafruit_LSM6DS3 Sensor;

        constexpr int EepromStartAddress = 0;

        std::optional<Vec::Mat3> mCalibration;
        /*
                std::optional<Calibration> TryLoadCalibration()
                {
                    Calibration calibration;
                    // Read the header
                    uint8_t header[ 3 ];
                    EEPROM.get( EepromStartAddress, header );
                    if( header[ 0 ] != 'C' || header[ 1 ] != 'A' || header[ 2 ] != 'L' )
                    {
                        return std::nullopt;
                    }

                    // Read the version
                    uint8_t version;
                    EEPROM.get( EepromStartAddress + 3, version );
                    if( version != 1 )
                    {
                        return std::nullopt;
                    }

                    // Read the calibration values
                    EEPROM.get( EepromStartAddress + 4, calibration.mX );
                    EEPROM.get( EepromStartAddress + 8, calibration.mY );
                    EEPROM.get( EepromStartAddress + 12, calibration.mZ );

                    return calibration;
                }

                void SaveCalibration( const Calibration& calibration )
                {
                    // calibration is stored as:
                    // header: "CAL"
                    // version: 1
                    // down values

                    assert( EEPROM.begin( 512 ) );
                    const uint8_t header[ 3 ] = { 'C', 'A', 'L' };
                    const uint8_t version = 1;
                    EEPROM.put( EepromStartAddress, header );
                    EEPROM.put( EepromStartAddress + 3, version );
                    EEPROM.put( EepromStartAddress + 4, calibration.mX );
                    EEPROM.put( EepromStartAddress + 8, calibration.mY );
                    EEPROM.put( EepromStartAddress + 12, calibration.mZ );
                    EEPROM.end();
                }
                    */

        void ComputeStateVectors( State& state, float x, float y, float z )
        {
            if( !mCalibration.has_value() )
            {
                state.mForward = z;
                state.mLeft = x;
                state.mUp = y;
                state.mCalibrated = false;
                return;
            }

            Vec::Vec3 v_s{ x, y, z };
            v_s = Vec::to_enclosure( *mCalibration, v_s );

            // LOG_INFO( "{%f, %f, %f} => {%f, %f, %f}", x, y, z, v_s.x, v_s.y, v_s.z );
            state.mForward = v_s.x;
            state.mLeft = v_s.y;
            state.mUp = v_s.z;
            state.mCalibrated = true;
        }
    }


    void Begin()
    {
        LOG_TRACE( "Motion::Begin()" );

        // try to load calibration.
        // mCalibration = TryLoadCalibration();
        if( mCalibration.has_value() )
        {
            // LOG_INFO( "Loaded calibration for Motion: X:{}; Y: {}; Z: {}", mCalibration->mX, mCalibration->mY, mCalibration->mZ );
        }
        else
        {
            LOG_WARN( "Failed to load calibration for Motion" );
        }

        if( !Sensor.begin_I2C() )
        {
            while( !Sensor.begin_I2C() )
            {
                LOG_ERROR( "failed to start I2C motion sensor" );
                delay( 1000 );
            }

            return;
        }
    }

    State GetState()
    {
        State state;
        state.mValid = false;
        float x, y, z = 0;
        // If the clock is mounted vertically on the dash, the native coordinate system is as follows:
        // +x = left
        // +y = up
        // +z = forward
        if( Sensor.readAcceleration( x, y, z ) != 1 )
        {
            return state;
        }
        ComputeStateVectors( state, x, y, z );
        state.mValid = true;

        // the clock is slightly tilted, so we'll need to adjust the forward vector.
        // it's rotated about Y, so X is pointing down about 15 degrees..

        // TODO: we'll need to rotate the forward vector about the X axis by 15 degrees.

        return state;
    }

    void Calibrate()
    {
        // this function will consider the current ACCEL data to be the down vector when parted on a flat surface.
        // it always assumes that the forward direction on the PCB is in the forward plane (no rotation compensation.)
        Vec::Vec3 calibration_vector;
        if( Sensor.readAcceleration( calibration_vector.x, calibration_vector.y, calibration_vector.z ) == 1 )
        {
            if( mCalibration.has_value() )
            {
                LOG_INFO( "Overwriting existing calibration..." );
            }
            LOG_INFO( "New calibration down vector: %f, %f, %f", calibration_vector.x, calibration_vector.y, calibration_vector.z );
            Vec::Vec3 front_axis{ 0, 1, 0 };
            mCalibration = Vec::calibrate_rotation( calibration_vector, front_axis );
            // SaveCalibration( calibration_vector );
            LOG_INFO( "Finished saving calibration" );
        }
        else
        {
            LOG_ERROR( "Failed to read accelerometer during calibration" );
        }
    }

    HistoryTracker::HistoryTracker( int16_t size, int32_t ms_per_tick )
        : mSize( size ), mMsPerTick( ms_per_tick ), mLastUpdateTime( 0 ), mBuffer( new double[ mSize ] )
    {
    }
    HistoryTracker::~HistoryTracker()
    {
        delete[] mBuffer;
    }

    void HistoryTracker::PushData( double data, int32_t timestamp )
    {
        if( timestamp - mLastUpdateTime >= mMsPerTick )
        {
            mLastUpdateTime = timestamp;

            mBuffer[ mHead ] = data;
            mHead = ( mHead + 1 ) % mSize;
            if( mCount < mSize )
            {
                mCount++;
            }
        }
    }

    void HistoryTracker::GetData( double* buffer, int16_t size )
    {
        int16_t to_copy = std::min( size, mCount );
        for( int16_t i = 0; i < to_copy; i++ )
        {
            // buffer[0] should contain the most recently pushed element, and buffer[1] the one before that, and so on.
            int16_t read_index = ( mHead + mSize - 1 - i ) % mSize;
            buffer[ i ] = mBuffer[ read_index ];
        }

        // fill the rest of the output with positive infinity:
        for( int16_t i = to_copy; i < size; i++ )
        {
            buffer[ i ] = std::numeric_limits<double>::infinity();
        }
    }

}