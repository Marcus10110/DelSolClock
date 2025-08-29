#pragma once

namespace Vec
{
    struct Vec3
    {
        float x, y, z;
    };

    struct Mat3
    {
        float m[ 3 ][ 3 ];
    };

    float dot( const Vec3& a, const Vec3& b );
    Vec3 cross( const Vec3& a, const Vec3& b );

    float norm( const Vec3& v );

    Vec3 normalize( const Vec3& v );

    Vec3 mul( const Mat3& M, const Vec3& v );

    Mat3 transpose( const Mat3& A );

    Mat3 compute_C_E_from_S( const Vec3& g_s, const Vec3& front_s );

    Mat3 calibrate_rotation( const Vec3& avg_g_s, const Vec3& sensor_front_axis );

    Vec3 to_enclosure( const Mat3& C_E_S, const Vec3& v_s );
}