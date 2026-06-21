#include "navigation_service.h"
#include "logger.h"
#include "utilities.h"

#include <Arduino.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <mutex>
#include <vector>

#include "route_codec.h"

// Navigation service: the phone writes a binary RouteSummary (route_codec format)
// in <=512-byte chunks; a chunk shorter than the chunk size ends the transfer.
// After each chunk we notify "continue"/"success"/"error" (same handshake as the
// OTA service). On success the accumulated bytes are decoded and stored.
#define DELSOL_NAVIGATION_SERVICE_UUID "77f5d2b5-efa1-4d55-b14a-cc92b72708a0"
#define DELSOL_NAV_ROUTE_CHARACTERISTIC_UUID "b9f0a2d1-6c3e-4a8b-9d27-1f5c0e6a4b30"

namespace NavigationService
{
    namespace
    {
        constexpr size_t FullPacketSize = 512;

        std::mutex Mutex;
        std::vector<uint8_t> RxBuffer;     // accumulating chunks
        uint32_t TransferStartMs{ 0 };

        nav::RouteSummary CurrentRoute;    // last successfully decoded route
        bool RouteReady{ false };
        bool NewRoutePending{ false };     // set on each new download; consumed by the loop

        class NavWriteCallbacks : public BLECharacteristicCallbacks
        {
          public:
            void onWrite( BLECharacteristic* characteristic ) override
            {
                std::string rxData = characteristic->getValue();
                const size_t len = rxData.length();

                size_t total = 0;
                {
                    std::scoped_lock lock( Mutex );
                    if( RxBuffer.empty() )
                    {
                        TransferStartMs = millis();
                        LOG_INFO( "nav: route transfer begin" );
                    }
                    RxBuffer.insert( RxBuffer.end(), rxData.begin(), rxData.end() );
                    total = RxBuffer.size();
                }

                // A short (or empty) chunk signals end of transfer.
                if( len != FullPacketSize )
                {
                    nav::RouteSummary decoded;
                    bool ok = false;
                    {
                        std::scoped_lock lock( Mutex );
                        ok = nav::decodeRoute( RxBuffer.data(), RxBuffer.size(), decoded );
                    }
                    auto duration = millis() - TransferStartMs;

                    if( ok )
                    {
                        {
                            std::scoped_lock lock( Mutex );
                            CurrentRoute = decoded;
                            RouteReady = true;
                            NewRoutePending = true;
                            RxBuffer.clear();
                        }
                        LOG_INFO( "nav: route OK %u bytes, %u pts, %u maneuvers, %u ms",
                                  ( unsigned )total, ( unsigned )decoded.polyline.size(),
                                  ( unsigned )decoded.maneuvers.size(), ( unsigned )duration );
                        characteristic->setValue( "success" );
                        characteristic->notify();
                    }
                    else
                    {
                        {
                            std::scoped_lock lock( Mutex );
                            RxBuffer.clear();
                        }
                        LOG_ERROR( "nav: route decode FAILED after %u bytes, %u ms",
                                   ( unsigned )total, ( unsigned )duration );
                        characteristic->setValue( "error" );
                        characteristic->notify();
                    }
                    return;
                }

                characteristic->setValue( "continue" );
                characteristic->notify();
            }
        };
    }

    void Begin( BLEServer* server )
    {
        PRINT_MEMORY_USAGE_MSG( "before NavigationService Begin" );
        auto service = server->createService( DELSOL_NAVIGATION_SERVICE_UUID );
        auto route_characteristic = service->createCharacteristic(
            DELSOL_NAV_ROUTE_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR );
        route_characteristic->setCallbacks( new NavWriteCallbacks() );
        route_characteristic->addDescriptor( new BLE2902() );
        service->start();
        LOG_INFO( "nav: service started" );
    }

    bool HasRoute()
    {
        std::scoped_lock lock( Mutex );
        return RouteReady;
    }

    bool ConsumeNewRoute()
    {
        std::scoped_lock lock( Mutex );
        bool pending = NewRoutePending;
        NewRoutePending = false;
        return pending;
    }

    const nav::RouteSummary& GetRoute()
    {
        return CurrentRoute;
    }
}
