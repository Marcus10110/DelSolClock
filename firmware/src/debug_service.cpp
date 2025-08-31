#include "debug_service.h"

#include "logger.h"

#include "BLE2902.h"
#include <BLEServer.h>
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_err.h"

#define DEBUG_SERVICE_UUID "2b0caa53-e543-4bb0-8f7f-50d1c64aa0dd"
#define DEBUG_STATUS_CHARACTERISTIC_UUID "32a18dc5-fdda-4601-b5b7-dc2920ac3f37"

namespace DebugService
{
    namespace
    {
        BLECharacteristic* DebugStatusCharacteristic = nullptr;

        bool CoreDumpIsAvailable( size_t* total_size_out = nullptr )
        {
            size_t addr = 0, size = 0;
            esp_err_t err = esp_core_dump_image_get( &addr, &size );
            if( err == ESP_OK && size > 0 )
            {
                if( total_size_out )
                    *total_size_out = size;
                return true;
            }
            return false;
        }


    }

    bool StartDebugService( BLEServer* server )
    {
        LOG_TRACE( "creating debug service" );
        auto debug_service = server->createService( DEBUG_SERVICE_UUID );
        LOG_TRACE( "creating debug status characteristic" );
        DebugStatusCharacteristic =
            debug_service->createCharacteristic( DEBUG_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        DebugStatusCharacteristic->addDescriptor( new BLE2902() );

        size_t core_size = 0;
        bool has_core = CoreDumpIsAvailable( &core_size );
        if( has_core )
        {
            LOG_INFO( "Core Dump available, advertising size: %d", core_size );
            std::string status_str = "CRASH:" + std::to_string( core_size );
            DebugStatusCharacteristic->setValue( status_str.c_str() );
        }
        else
        {
            LOG_TRACE( "no core dump available" );
            DebugStatusCharacteristic->setValue( "NOCRASH" );
        }
        LOG_TRACE( "starting debug service" );
        debug_service->start();
        return true;
    }
}