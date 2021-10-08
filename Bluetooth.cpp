#include "Bluetooth.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <BLE2904.h>

#include <time.h>
#include <sys/time.h>

#include "AppleMediaService.h"
#include "CurrentTimeService.h"
#include "AppleNotificationCenterService.h"

#define APPLE_SERVICE_UUID "89D3502B-0F36-433A-8EF4-C502AD55F8DC"

#define DELSOL_VEHICLE_SERVICE_UUID "8fb88487-73cf-4cce-b495-505a4b54b802"
#define DELSOL_STATUS_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_BETTERY_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_LOCATION_SERVICE_UUID "61d33c70-e3cd-4b31-90d8-a6e14162fffd"
#define DELSOL_NAVIGATION_SERVICE_UUID "77f5d2b5-efa1-4d55-b14a-cc92b72708a0"

namespace Bluetooth
{
    namespace
    {
        bool DeviceConnected = false;
        bool OldDeviceConnected = false;
        bool AuthenticationComplete = false;
        bool Ended = true;
        BLEServer* Server = nullptr;
        BLEAddress* IPhoneAddress = nullptr;
        BLEClient* Client = nullptr;
        BLESecurity Security{};

        RTC_DATA_ATTR bool TimeSet = false;

        class GetAddressServerCallbacks : public BLEServerCallbacks
        {
          public:
            void onConnect( BLEServer* server, esp_ble_gatts_cb_param_t* param ) override
            {
                Serial.println( "onConnect CB" );
                if( IPhoneAddress )
                {
                    delete IPhoneAddress;
                    IPhoneAddress = nullptr;
                }
                IPhoneAddress = new BLEAddress( param->connect.remote_bda );

                DeviceConnected = true;
            };

            void onDisconnect( BLEServer* server ) override
            {
                Serial.println( "onDisconnect CB" );
                DeviceConnected = false;
            }
        };

        class NotificationSecurityCallbacks : public BLESecurityCallbacks
        {
            uint32_t onPassKeyRequest() override
            {
                Serial.println( "PassKeyRequest" );
                return 123456;
            }
            void onPassKeyNotify( uint32_t pass_key ) override
            {
                Serial.printf( "On passkey Notify number:%d\n", pass_key );
            }

            bool onSecurityRequest() override
            {
                Serial.println( "On Security Request" );
                return true;
            }

            bool onConfirmPIN( unsigned int ) override
            {
                Serial.println( "On Confirmed Pin Request" );
                return true;
            }

            void onAuthenticationComplete( esp_ble_auth_cmpl_t cmpl ) override
            {
                if( cmpl.success )
                {
                    Serial.println( "Authentication Successful!" );
                    uint16_t length;
                    esp_ble_gap_get_whitelist_size( &length );
                    Serial.printf( "size: %d\n", length );
                    AuthenticationComplete = true;
                }
                else
                {
                    Serial.println( "Authentication failed" );
                }
            }
        };


        void setServiceSolicitation( BLEAdvertisementData& advertisement_data, BLEUUID uuid )
        {
            char c_data[ 2 ];
            switch( uuid.bitSize() )
            {
            case 16:
            {
                // [Len] [0x14] [UUID16] data
                c_data[ 0 ] = 3;
                c_data[ 1 ] = ESP_BLE_AD_TYPE_SOL_SRV_UUID; // 0x14
                advertisement_data.addData( std::string( c_data, 2 ) + std::string( ( char* )&uuid.getNative()->uuid.uuid16, 2 ) );
                break;
            }

            case 128:
            {
                // [Len] [0x15] [UUID128] data
                c_data[ 0 ] = 17;
                c_data[ 1 ] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID; // 0x15
                advertisement_data.addData( std::string( c_data, 2 ) + std::string( ( char* )uuid.getNative()->uuid.uuid128, 16 ) );
                break;
            }

            default:
                return;
            }
        }


        void HandleConnection()
        {
            if( !IPhoneAddress )
                return;

            if( Client )
            {
                Serial.println( "HandleConnection: Client already exists, deleteing." );
                delete Client;
                Client = nullptr;
            }

            Client = BLEDevice::createClient();

            Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
            Security.setCapability( ESP_IO_CAP_IO );
            Security.setRespEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK );
            Serial.println( "set security details" );

            if( !Client->connect( *IPhoneAddress ) )
            {
                Serial.println( "failed to connect" );
                delete Client;
                Client = nullptr;
                return;
            }
            Serial.println( "connected using client!" );

            Serial.print( "Waiting for authentication" );
            while( !AuthenticationComplete && Client->isConnected() )
            {
                Serial.print( "." );
                delay( 100 );
            }
            Serial.println( "Authentication finished" );
            delay( 100 );
            if( !Client->isConnected() )
            {
                Serial.println( "client disconnected during authentication." );
                delete Client;
                Client = nullptr;
                return;
            }

            if( !AppleMediaService::StartMediaService( Client ) )
            {
                Client->disconnect();
                delete Client;
                Client = nullptr;
                Serial.println( "StartMediaService failed" );
                return;
            }

            CurrentTimeService::CurrentTime time;
            if( !CurrentTimeService::StartTimeService( Client, &time ) )
            {
                Serial.println( "StartTimeService failed" );
                return;
            }
            timeval new_time;
            new_time.tv_sec = time.ToTimeT();
            new_time.tv_usec = static_cast<long>( time.mSecondsFraction * 1000000 );
            Serial.print( "Unix time stamp: " );
            Serial.print( new_time.tv_sec );
            Serial.print( ", usec: " );
            Serial.println( new_time.tv_usec );
            if( settimeofday( &new_time, nullptr ) != 0 )
            {
                Serial.println( "Error setting time of day" );
            }
            else
            {
                TimeSet = true;
            }

            time.Dump();
// Apple Notification Service disabled, not used.
#if 0
            if( !AppleNotifications::StartNotificationService( Client ) )
            {
                Serial.println( "failed to start the notification service" );
                return;
            }
            Serial.println( "Notification service started" );
#endif
        }


    }


    void Begin()
    {
        Ended = false;
        BLEDevice::init( "Del Sol" );
        Server = BLEDevice::createServer();
        Server->setCallbacks( new GetAddressServerCallbacks() );


        auto vechicle_service = Server->createService( DELSOL_VEHICLE_SERVICE_UUID );
        auto vehicle_status_characteristic = vechicle_service->createCharacteristic(
            DELSOL_STATUS_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY );

        int vehicle_status = 0xAA;
        vehicle_status_characteristic->setValue( vehicle_status );
        vechicle_service->start();


        BLEDevice::setEncryptionLevel( ESP_BLE_SEC_ENCRYPT );
        BLEDevice::setSecurityCallbacks( new NotificationSecurityCallbacks() );


        BLEAdvertising* advertising = Server->getAdvertising();
        advertising->setAppearance( 0x03C1 );
        advertising->setScanResponse( true );
        BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
        oAdvertisementData.setFlags( 0x01 );
        setServiceSolicitation( oAdvertisementData, BLEUUID( APPLE_SERVICE_UUID ) );
        setServiceSolicitation( oAdvertisementData, BLEUUID( "7905F431-B5CE-4E99-A40F-4B1E122D00D0" ) );
        advertising->setAdvertisementData( oAdvertisementData );

        // TODO: Figure out what this does, and why we set it twice.
        Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
        Security.setCapability( ESP_IO_CAP_OUT ); // This value changes between server and client. Is it needed?
        Security.setInitEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK ); // not sure how this is used either.
        advertising->start();
    }
    void End()
    {
        Ended = true;
        if( Client )
        {
            Client->disconnect();
            delete Client;
            Client = nullptr;
        }
        DeviceConnected = false;
        OldDeviceConnected = false;
        if( Server )
        {
            auto advertising = Server->getAdvertising();
            if( advertising )
            {
                advertising->stop();
            }
            delete Server;
            Server = nullptr;
        }

        if( IPhoneAddress )
        {
            delete IPhoneAddress;
            IPhoneAddress = nullptr;
        }

        BLEDevice::deinit( true );
    }

    void Service()
    {
        if( Ended )
        {
            return;
        }
        if( !DeviceConnected && OldDeviceConnected )
        {
            // device disconnected.
            delay( 500 ); // give the bluetooth stack the chance to get things ready
            if( Client )
            {
                Client->disconnect();
                delete Client;
                Client = nullptr;
            }
            AuthenticationComplete = false;
            Server->startAdvertising(); // restart advertising
            Serial.println( "disconnected, restart advertising" );
            OldDeviceConnected = DeviceConnected;
        }
        // connecting
        else if( DeviceConnected && !OldDeviceConnected )
        {
            // do stuff here on connecting
            OldDeviceConnected = DeviceConnected;
            Serial.println( "connected!" );
            HandleConnection();
            Serial.println( "HandleConnection returned" );
        }
    }

    bool IsConnected()
    {
        return DeviceConnected && OldDeviceConnected;
    }

    bool IsTimeSet()
    {
        return TimeSet;
    }
}