#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <BLE2904.h>


#define APPLE_SERVICE_UUID "89D3502B-0F36-433A-8EF4-C502AD55F8DC"
#define APPLE_REMOTE_COMMAND_UUID "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2"
#define APPLE_ENTITY_UPDATE_UUID "2F7CABCE-808D-411F-9A0C-BB92BA96C102"
#define APPLE_ENTITY_ATTRIBUTE_UUID "C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7"

static const BLEUUID GATT_CTS_UUID( ( uint16_t )0x1805 );
static const BLEUUID GATT_CTS_CTC_UUID( ( uint16_t )0x2A2B );


#define DELSOL_VEHICLE_SERVICE_UUID "8fb88487-73cf-4cce-b495-505a4b54b802"
#define DELSOL_STATUS_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_BETTERY_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"
#define DELSOL_LOCATION_SERVICE_UUID "61d33c70-e3cd-4b31-90d8-a6e14162fffd"
#define DELSOL_NAVIGATION_SERVICE_UUID "77f5d2b5-efa1-4d55-b14a-cc92b72708a0"


void PrintBytes( const std::string& byte_str )
{
    Serial.printf( "[%i]: ", byte_str.size() );
    for( uint8_t c : byte_str )
    {
        if( c <= 0x0f )
        {
            Serial.print( "0" );
        }
        Serial.print( c, 16 );
    }
    Serial.println( "" );
}


bool deviceConnected = false;
bool oldDeviceConnected = false;
bool AuthenticationComplete = false;
BLEServer* pServer = nullptr;
BLEAddress* iphone_address = nullptr;
BLEClient* client = nullptr;
BLESecurity Security{};

class MyServerCallbacks : public BLEServerCallbacks
{
  public:
    void onConnect( BLEServer* pServer, esp_ble_gatts_cb_param_t* param ) override
    {
        Serial.println( "onConnect CB" );
        if( iphone_address )
        {
            delete iphone_address;
            iphone_address = nullptr;
        }
        iphone_address = new BLEAddress( param->connect.remote_bda );

        deviceConnected = true;
    };

    void onDisconnect( BLEServer* pServer ) override
    {
        Serial.println( "onDisconnect CB" );
        deviceConnected = false;
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
        Serial.println( "Authentication Complete!" );
        if( cmpl.success )
        {
            uint16_t length;
            esp_ble_gap_get_whitelist_size( &length );
            Serial.printf( "size: %d\n", length );
        }
        AuthenticationComplete = true;
    }
};


void setup()
{
    Serial.begin( 115200 );
    Serial.println( "Del Sol Clock Booting" );

    BLEDevice::init( "Del Sol" );
    pServer = BLEDevice::createServer();
    pServer->setCallbacks( new MyServerCallbacks() );

    auto vechicle_service = pServer->createService( DELSOL_VEHICLE_SERVICE_UUID );
    auto vehicle_status_characteristic = vechicle_service->createCharacteristic(
        DELSOL_STATUS_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY );

    int vehicle_status = 0xAA;
    vehicle_status_characteristic->setValue( vehicle_status );
    vechicle_service->start();


    BLEDevice::setEncryptionLevel( ESP_BLE_SEC_ENCRYPT );
    BLEDevice::setSecurityCallbacks( new NotificationSecurityCallbacks() );


    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance( 0x03C1 );
    pAdvertising->setScanResponse( true );
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags( 0x01 );
    setServiceSolicitation( oAdvertisementData, BLEUUID( APPLE_SERVICE_UUID ) );
    pAdvertising->setAdvertisementData( oAdvertisementData );


    Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
    Security.setCapability( ESP_IO_CAP_OUT ); // This value changes between server and client. Is it needed?
    Security.setInitEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK ); // not sure how this is used either.
    pAdvertising->start();
}

BLERemoteCharacteristic* entity_update = nullptr;

void loop()
{
    // put your main code here, to run repeatedly:
    // disconnecting
    if( !deviceConnected && oldDeviceConnected )
    {
        delay( 500 ); // give the bluetooth stack the chance to get things ready
        if( client )
        {
            client->disconnect();
            delete client;
            client = nullptr;
        }
        AuthenticationComplete = false;
        pServer->startAdvertising(); // restart advertising
        Serial.println( "disconnected, restart advertising" );
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    else if( deviceConnected && !oldDeviceConnected )
    {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
        Serial.println( "connected!" );
        HandleConnection();
        Serial.println( "HandleConnection returned" );
    }
    else if( deviceConnected )
    {
        delay( 2000 );
        Serial.print( "." );
        if( entity_update )
        {
            // auto val = entity_update->readValue();
            // Serial.println( "reading value..." );
            // Serial.println( val.c_str() );
        }
    }
    else
    {
        delay( 100 );
    }
}

void setServiceSolicitation( BLEAdvertisementData& advertisementData, BLEUUID uuid )
{
    char cdata[ 2 ];
    switch( uuid.bitSize() )
    {
    case 16:
    {
        // [Len] [0x14] [UUID16] data
        cdata[ 0 ] = 3;
        cdata[ 1 ] = ESP_BLE_AD_TYPE_SOL_SRV_UUID; // 0x14
        advertisementData.addData( std::string( cdata, 2 ) + std::string( ( char* )&uuid.getNative()->uuid.uuid16, 2 ) );
        break;
    }

    case 128:
    {
        // [Len] [0x15] [UUID128] data
        cdata[ 0 ] = 17;
        cdata[ 1 ] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID; // 0x15
        advertisementData.addData( std::string( cdata, 2 ) + std::string( ( char* )uuid.getNative()->uuid.uuid128, 16 ) );
        break;
    }

    default:
        return;
    }
}

void UpdateHandler( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
    Serial.println( "got data!" );
    Serial.println( "got data!b" );
    Serial.println( "got data!c" );
    /*
        std::string data( reinterpret_cast<char*>( pData ), length );
        for( int i = 0; i < length; ++i )
        {
            Serial.print( ( char )( pData[ i ] ) );
        }

        Serial.println( "" );
        */
}

void HandleConnection()
{
    if( !iphone_address )
        return;

    client = BLEDevice::createClient();

    Security.setAuthenticationMode( ESP_LE_AUTH_REQ_SC_BOND );
    Security.setCapability( ESP_IO_CAP_IO );
    Security.setRespEncryptionKey( ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK );
    Serial.println( "set security details" );

    if( !client->connect( *iphone_address ) )
    {
        Serial.println( "failed to connect" );
        delete client;
        client = nullptr;
        return;
    }
    Serial.println( "connected using client!" );

    Serial.print( "Waiting for authentication" );
    while( !AuthenticationComplete )
    {
        Serial.print( "." );
        delay( 100 );
    }
    Serial.println( "Authentication complete" );
    delay( 100 );
    auto music_service = client->getService( BLEUUID( APPLE_SERVICE_UUID ) );
    if( !music_service || !deviceConnected )
    {
        client->disconnect();
        delete client;
        client = nullptr;
        Serial.println( "Apple music service not found" );
        return;
    }
    Serial.println( "found service" );


    auto remote_command_characteristic = music_service->getCharacteristic( APPLE_REMOTE_COMMAND_UUID );
    remote_command_characteristic->registerForNotify( []( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length,
                                                          bool isNotify ) { Serial.println( "event from remote command" ); } );

    auto entity_attribute_characteristic = music_service->getCharacteristic( APPLE_REMOTE_COMMAND_UUID );
    // TODO:
    // get the one descriptor we're interested in.
    // subscribe.
    // set value.
    // relaxt!!
    entity_update = music_service->getCharacteristic( APPLE_ENTITY_UPDATE_UUID );
    if( !entity_update || !deviceConnected )
    {
        Serial.println( "entity_update not found" );
        client->disconnect();
        delete client;
        client = nullptr;
        return;
    }
    Serial.println( "found entity_update" );
    entity_update->registerForNotify(
        []( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
        {
            Serial.println( "event from entity update" );
            PrintBytes( std::string( ( char* )pData, length ) );
            if( length > 3 )
            {
                std::string value( ( char* )( pData + 3 ), length - 3 );
                Serial.println( value.c_str() );
            }
            Serial.println( "done!" );
        } );
    // entity_update->registerForNotify( UpdateHandler );
    Serial.println( "done registering for updates" );
    const uint8_t v[] = { 0x1, 0x0 };
    // entity_update->getDescriptor( BLEUUID( ( uint16_t )0x2902 ) )->writeValue( ( uint8_t* )v, 2, true );
    delay( 500 );

    uint8_t write_data_1[] = { 2, 0, 1, 2, 3 };
    uint8_t write_data_2[] = { 0, 0 };

    Serial.println( "writing initial value..." );
    entity_update->writeValue( ( uint8_t* )write_data_1, sizeof( write_data_1 ), true );

    Serial.println( "..." );
    entity_update->writeValue( ( uint8_t* )write_data_2, sizeof( write_data_2 ), true );
    delay( 5 );
    Serial.println( "totally done!A" );
    Serial.println( "totally done!B" );
    Serial.println( entity_update->toString().c_str() );
    Serial.println( "\n\n" );
    Serial.println( client->toString().c_str() );

    BLERemoteService* remote_time_service = client->getService( GATT_CTS_UUID );
    if( !remote_time_service )
    {
        return;
    }

    BLERemoteCharacteristic* time_characteristic = remote_time_service->getCharacteristic( GATT_CTS_CTC_UUID );

    time_characteristic->readUInt8();
    auto data = time_characteristic->readRawData();
    int year = data[ 0 ] | ( data[ 1 ] << 8 );
    int month = data[ 2 ];
    int day = data[ 3 ];
    int hour = data[ 4 ];
    int minute = data[ 5 ];
    int second = data[ 6 ];
    // int weekday = data[7];
    int frac256 = data[ 8 ];
    Serial.printf( "%i/%i/%i, %i:%i:%i\n", month, day, year, hour, minute, second );
}

/*
Application Notes:

Let's display the connection status on screen. (connected / not connected)
Let's display the current song information if available. (playing)
Let's display the current time, and update the current time at the beginning of every connection.

Let's see if notifications include driving directions

Let's make available a few flags - window, roof and trunk, interior lights voltage?, and ignition. battery voltage too.
Let's expose the current GPS location

Let's only attempt to connect when the ignition is on. let's disconnect when it's off, and go into sleep mode (turn off screen too)

State machine:

Is ignition on?
- start or continue BT operation

Is ignition off?
- clean up or stop advertising BT.
*/