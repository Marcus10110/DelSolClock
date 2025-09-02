#include "debug_service.h"

#include "logger.h"
#include "utilities.h"

#include "BLE2902.h"
#include <BLEServer.h>
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_err.h"

#include <atomic>

#define DEBUG_SERVICE_UUID "2b0caa53-e543-4bb0-8f7f-50d1c64aa0dd"
#define DEBUG_STATUS_CHARACTERISTIC_UUID "32a18dc5-fdda-4601-b5b7-dc2920ac3f37"
#define DEBUG_DATA_CHARACTERISTIC_UUID "2969eccf-48f1-4069-a662-1ae77fe69118"
#define DEBUG_CONTROL_CHARACTERISTIC_UUID "65376e10-7797-435b-ac52-14ac0fab362c"

namespace DebugService
{
    namespace
    {
        BLECharacteristic* DebugStatusCharacteristic = nullptr;
        BLECharacteristic* DebugDataCharacteristic = nullptr;
        BLECharacteristic* DebugControlCharacteristic = nullptr;

        std::atomic<bool> ShouldAssert = false;

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

        constexpr size_t SliceSize = 510;
        uint8_t CoreDumpBuffer[ 512 ] = { 0 };
        const esp_partition_t* DumpPartition = nullptr;
        // this is the address, relative to the partition start, where the dump begins.
        size_t DumpStartPartitionRelative = 0;
        size_t DumpSize = 0;
        int CurrentSlice = 0;
        int SliceCount = 0;

        int GetSliceCount( size_t total_size )
        {
            return ( total_size + SliceSize - 1 ) / SliceSize;
        }

        void GetSliceAddress( size_t total_size, int slice, int* slice_start, int* slice_length )
        {
            // return the start index and slice length (max SliceSize) for the given slice index.
            if( slice_start != nullptr )
            {
                *slice_start = slice * SliceSize;
            }
            if( slice_length != nullptr )
            {
                *slice_length = std::min( SliceSize, total_size - *slice_start );
            }
        }

        void WriteSliceToCharacteristic( int slice )
        {
            int slice_start = 0;
            int slice_length = 0;
            GetSliceAddress( DumpSize, slice, &slice_start, &slice_length );
            if( slice_length > 0 )
            {
                // Read the slice from the core dump buffer and write it to the characteristic.

                esp_err_t er =
                    esp_partition_read( DumpPartition, DumpStartPartitionRelative + slice_start, &CoreDumpBuffer[ 2 ], slice_length );
                if( er != ESP_OK )
                {
                    LOG_ERROR( "Failed to read core dump slice (%i, %i), error: %i", slice_start, slice_length, er );
                    return;
                }

                // write the current slice index into the first 2 bytes of the array, little endian:
                CoreDumpBuffer[ 0 ] = CurrentSlice & 0xFF;
                CoreDumpBuffer[ 1 ] = ( CurrentSlice >> 8 ) & 0xFF;
                LOG_TRACE( "write dmp slice %i, length %i", CurrentSlice, slice_length );
                DebugDataCharacteristic->setValue( CoreDumpBuffer, slice_length + 2 );
            }
        }

        bool LoadPartitionData()
        {
            // load partition details.
            // Find the coredump partition; name is usually "coredump" but we match by subtype.
            const esp_partition_t* part = esp_partition_find_first( ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL );
            if( !part )
            {
                LOG_ERROR( "Failed to find coredump partition" );
                return false;
            }

            size_t addr = 0, size = 0;
            esp_err_t err = esp_core_dump_image_get( &addr, &size );

            if( addr < part->address )
            {
                LOG_ERROR( "Core dump image address is before partition start" );
                return false;
            }
            size_t part_offset = addr - part->address;
            if( part_offset + size > part->size )
            {
                LOG_ERROR( "Core dump image size is larger than partition size" );
                return false;
            }

            // only assign this if everything is valid
            DumpStartPartitionRelative = part_offset;
            DumpPartition = part;
            DumpSize = size;
            LOG_INFO( "Core dump image loaded. Address: 0x%08X, Size: %d relative address: %d", addr, size, DumpStartPartitionRelative );
            return true;
        }

        class DebugDataCallbacks : public BLECharacteristicCallbacks
        {
            void onRead( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                if( CurrentSlice < SliceCount )
                {
                    WriteSliceToCharacteristic( CurrentSlice );
                    CurrentSlice++;
                    if( CurrentSlice >= SliceCount )
                    {
                        CurrentSlice = 0;
                    }
                }
            }
        };

        DebugDataCallbacks DebugDataCB;

        class DebugControlCallbacks : public BLECharacteristicCallbacks
        {
            void onWrite( BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param ) override
            {
                std::string rxValue = pCharacteristic->getValue();
                LOG_TRACE( "Debug control characteristic received value: %s", rxValue.c_str() );

                if( rxValue == "REBOOT" )
                {
                    LOG_INFO( "Rebooting device..." );
                    esp_restart();
                    return;
                }
                else if( rxValue == "ASSERT" )
                {
                    LOG_INFO( "Asserting device now..." );
                    // crash such that we generate a nice clean stack trace to right here.
                    abort();
                }
                else if( rxValue == "ASSERT_LATER" )
                {
                    LOG_INFO( "setting flag to assert later..." );
                    // assert from the main thread instead.
                    ShouldAssert = true;
                }
                else if( rxValue == "CLEAR" )
                {
                    LOG_INFO( "Clearing crash data..." );
                    esp_core_dump_image_erase();
                    DebugStatusCharacteristic->setValue( "NOCRASH" );
                    LOG_INFO( "crash data cleared." );
                }
                else if( rxValue == "PRINT" )
                {
                    LOG_INFO( "Log INFO" );
                }
                else
                {
                    LOG_WARN( "Unknown command received: %s", rxValue.c_str() );
                }
            }
        };

        DebugControlCallbacks DebugControlCB;

    }


    bool StartDebugService( BLEServer* server )
    {
        LOG_TRACE( "creating debug service" );
        PRINT_MEMORY_USAGE();
        auto debug_service = server->createService( BLEUUID( DEBUG_SERVICE_UUID ) );
        LOG_TRACE( "creating debug status characteristic" );
        DebugStatusCharacteristic =
            debug_service->createCharacteristic( DEBUG_STATUS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        DebugStatusCharacteristic->addDescriptor( new BLE2902() );


        LOG_TRACE( "creating debug data characteristic" );
        DebugDataCharacteristic = debug_service->createCharacteristic( DEBUG_DATA_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        DebugDataCharacteristic->addDescriptor( new BLE2902() );
        DebugDataCharacteristic->setCallbacks( &DebugDataCB );

        DebugControlCharacteristic =
            debug_service->createCharacteristic( DEBUG_CONTROL_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE );
        DebugControlCharacteristic->addDescriptor( new BLE2902() );
        DebugControlCharacteristic->setCallbacks( &DebugControlCB );

        size_t core_size = 0;
        bool has_core = CoreDumpIsAvailable( &core_size );
        if( has_core )
        {
            SliceCount = GetSliceCount( core_size );

            LOG_INFO( "Core Dump available, advertising size: %d", core_size );
            std::string status_str = "CRASH:" + std::to_string( core_size ) + ":" + std::to_string( SliceCount );
            DebugStatusCharacteristic->setValue( status_str.c_str() );

            if( !LoadPartitionData() )
            {
                LOG_ERROR( "Failed to load partition data" );
                return false;
            }

            WriteSliceToCharacteristic( CurrentSlice );
        }
        else
        {
            LOG_TRACE( "no core dump available" );
            DebugStatusCharacteristic->setValue( "NOCRASH" );
        }
        LOG_TRACE( "starting debug service" );
        debug_service->start();
        LOG_TRACE( "Debug service started" );
        PRINT_MEMORY_USAGE();
        return true;
    }

    void HandleAssertCase()
    {
        if( ShouldAssert.load() )
        {
            LOG_INFO( "Asserting device now..." );
            ShouldAssert = false;
            // crash such that we generate a nice clean stack trace to right here.
            abort();
        }
    }
}