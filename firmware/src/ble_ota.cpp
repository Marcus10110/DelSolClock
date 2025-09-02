#include "ble_ota.h"
#include "version.h"
#include "logger.h"
#include "utilities.h"

#include <Arduino.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <mutex>

#include "esp_ota_ops.h"

#define FIRMWARE_UPDATE_SERVICE_UUID "69da0f2b-43a4-4c2a-b01d-0f11564c732b"
#define FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID "7efc013a-37b7-44da-8e1c-06e28256d83b"
#define FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID "a5c0d67a-9576-47ea-85c6-0347f8473cf3"


namespace BleOta
{
    namespace
    {
        esp_ota_handle_t UpdateHandle = 0;

        uint32_t PacketsReceived{ 0 };
        uint32_t FirstPacketTimeMs{ 0 };
        uint32_t LastPacketTimeMs{ 0 };

        constexpr uint32_t FullOtaPacketSize = 512;

        // cross thread data
        std::mutex Mutex;
        uint32_t BytesReceived{ 0 };
        bool RebootRequired{ false };
        uint32_t RebootMinTimeMs{ 0 };
        class OtaWriteCallbacks : public BLECharacteristicCallbacks
        {
          public:
            void onWrite( BLECharacteristic* characteristic ) override
            {
                auto start_time = millis();
                esp_err_t status;
                std::string rxData = characteristic->getValue();

                uint32_t bytes_received = 0;
                {
                    std::scoped_lock lock( Mutex );
                    if( BytesReceived == 0 )
                    {
                        LOG_TRACE( "BeginOTA" );
                        FirstPacketTimeMs = millis();
                    }
                    BytesReceived += rxData.length();
                    bytes_received = BytesReceived;
                }


                if( rxData.length() > 0 )
                {
                    status = esp_ota_write( UpdateHandle, rxData.c_str(), rxData.length() );
                    if( status != ESP_OK )
                    {
                        LOG_ERROR( "esp_ota_write: %i", status );
                        // TODO handle error;
                    }
                }
                auto write_time = millis() - start_time;
                if( rxData.length() != FullOtaPacketSize && bytes_received > 0 )
                {
                    LOG_TRACE( "EndOTA" );
                    status = esp_ota_end( UpdateHandle );
                    if( status != ESP_OK )
                    {
                        LOG_ERROR( "esp_ota_end error: %i", status );
                        // TODO handle error;
                    }
                    status = esp_ota_set_boot_partition( esp_ota_get_next_update_partition( NULL ) );
                    LOG_TRACE( "esp_ota_set_boot_partition: %i", status );
                    if( status == ESP_OK )
                    {
                        // the PC never gets this. I think we need to return from this callback before we reboot the device.
                        characteristic->setValue( "success" );
                        characteristic->notify();
                        LOG_TRACE( "notified success" );

                        {
                            std::scoped_lock lock( Mutex );
                            RebootMinTimeMs = millis() + 3000;
                            RebootRequired = true;
                        }
                        return;
                    }
                    else
                    {
                        {
                            std::scoped_lock lock( Mutex );
                            BytesReceived = 0;
                        }
                        LOG_ERROR( "Upload Error" );
                        characteristic->setValue( "error" );
                        characteristic->notify();
                        LOG_TRACE( "notified error" );
                        return;
                    }
                }

                characteristic->setValue( "continue" );
                characteristic->notify();

                uint32_t packet_time = 0;
                if( LastPacketTimeMs > 0 )
                {
                    packet_time = millis() - LastPacketTimeMs;
                }
                LastPacketTimeMs = millis();

                auto cycle_time = millis() - start_time;
                auto total_time = millis() - FirstPacketTimeMs;
                PacketsReceived++;
                float ms_per_cycle = static_cast<float>( total_time ) / PacketsReceived;
                LOG_TRACE( "OTA %ims/%ims/%ims %1.1f %u B", cycle_time, packet_time, total_time, ms_per_cycle, bytes_received );
            }
        };
    }
    void Begin( BLEServer* server )
    {
        LOG_TRACE( "Starting OTA service" );
        PRINT_MEMORY_USAGE();
        auto status = esp_ota_begin( esp_ota_get_next_update_partition( NULL ), OTA_SIZE_UNKNOWN, &UpdateHandle );
        LOG_TRACE( "esp_ota_begin: %i", status );
        if( status != ESP_OK )
        {
            return;
        }
        auto service = server->createService( FIRMWARE_UPDATE_SERVICE_UUID );
        auto write_characteristic = service->createCharacteristic(
            FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR );
        write_characteristic->setCallbacks( new OtaWriteCallbacks() );
        auto version_characteristic =
            service->createCharacteristic( FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ );
        write_characteristic->addDescriptor( new BLE2902() );

        version_characteristic->setValue( VERSION );
        service->start();
        LOG_TRACE( "Started OTA service. Current version: %s", VERSION );
        PRINT_MEMORY_USAGE();
    }

    bool IsInProgress( uint32_t* bytes_received )
    {
        std::scoped_lock lock( Mutex );
        if( bytes_received != nullptr )
        {
            *bytes_received = BytesReceived;
        }
        return BytesReceived > 0;
    }

    bool IsRebootRequired()
    {
        std::scoped_lock lock( Mutex );
        if( RebootRequired && millis() >= RebootMinTimeMs )
        {
            return true;
        }
        return false;
    }
}