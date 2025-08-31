#include "vec.h"
#include <cmath>
namespace Vec
{
    float dot( const Vec3& a, const Vec3& b )
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    Vec3 cross( const Vec3& a, const Vec3& b )
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    float norm( const Vec3& v )
    {
        return std::sqrt( dot( v, v ) );
    }
    Vec3 normalize( const Vec3& v )
    {
        float n = norm( v );
        if( n <= 1e-12f )
            return { 0, 0, 0 };
        return { v.x / n, v.y / n, v.z / n };
    }
    Vec3 mul( const Mat3& M, const Vec3& v )
    {
        return { M.m[ 0 ][ 0 ] * v.x + M.m[ 0 ][ 1 ] * v.y + M.m[ 0 ][ 2 ] * v.z,
                 M.m[ 1 ][ 0 ] * v.x + M.m[ 1 ][ 1 ] * v.y + M.m[ 1 ][ 2 ] * v.z,
                 M.m[ 2 ][ 0 ] * v.x + M.m[ 2 ][ 1 ] * v.y + M.m[ 2 ][ 2 ] * v.z };
    }
    Mat3 transpose( const Mat3& A )
    {
        Mat3 T{};
        for( int r = 0; r < 3; ++r )
            for( int c = 0; c < 3; ++c )
                T.m[ r ][ c ] = A.m[ c ][ r ];
        return T;
    }

    // Compute rotation C_{E<-S} using one gravity sample and known sensor-front axis.
    // Inputs:
    //   g_s       : accelerometer (avg) while device is still on a flat table (in g units)
    //   front_s   : sensor's "front" axis in sensor coords (e.g. {1,0,0} if +X is front)
    // Returns:
    //   C_E_S     : rotation mapping sensor vectors into enclosure frame (Forward,Right,Up)
    Mat3 compute_C_E_from_S( const Vec3& g_s, const Vec3& front_s )
    {
        // Up in sensor coords:
        Vec3 u_s = normalize( { -g_s.x, -g_s.y, -g_s.z } );

        // Project sensor-front onto horizontal plane (perp to Up):
        float f_vert = dot( front_s, u_s );
        Vec3 f_flat = { front_s.x - f_vert * u_s.x, front_s.y - f_vert * u_s.y, front_s.z - f_vert * u_s.z };
        f_flat = normalize( f_flat );

        // Handle near-degenerate case
        if( norm( f_flat ) < 1e-3f )
        {
            // If front is nearly vertical, you need another known axis (e.g., board "right").
            // As a mild fallback, pick any axis orthogonal to u_s:
            Vec3 tmp = std::fabs( u_s.x ) < 0.9f ? Vec3{ 1, 0, 0 } : Vec3{ 0, 1, 0 };
            f_flat = normalize( cross( tmp, u_s ) ); // some horizontal forward
        }

        // Right = Up x Forward (right-handed)
        Vec3 r_s = normalize( cross( u_s, f_flat ) );
        // Re-orthogonalized Up = Forward x Right
        Vec3 u_s_orth = cross( f_flat, r_s );

        // Columns are (Forward, Right, Up) in sensor coords -> C_{S<-E}
        Mat3 C_S_E{ { { f_flat.x, r_s.x, u_s_orth.x }, { f_flat.y, r_s.y, u_s_orth.y }, { f_flat.z, r_s.z, u_s_orth.z } } };

        // We want C_{E<-S} = (C_{S<-E})^T
        return transpose( C_S_E );
    }

    // Example usage:
    // 1) During calibration (device flat & still), average e.g. 1–2 seconds of accel:
    Mat3 calibrate_rotation( const Vec3& avg_g_s, const Vec3& sensor_front_axis )
    {
        return compute_C_E_from_S( avg_g_s, normalize( sensor_front_axis ) );
    }

    // 2) At runtime, convert any sensor vector v_s (accel, gyro, mag) into enclosure coords:
    Vec3 to_enclosure( const Mat3& C_E_S, const Vec3& v_s )
    {
        return mul( C_E_S, v_s );
    }

}