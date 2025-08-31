#include "gps.h"
#include "pins.h"
#include <TinyGPSPlus.h>
#include "driver/uart.h"
#include "logger.h"

namespace Gps
{
    namespace
    {
        constexpr uart_port_t GpsUartPort = UART_NUM_2;
        QueueHandle_t GpsUartQueue;
        TinyGPSPlus Gps;
        // constexpr char StandbyCommand[] = "$PMTK161,0*28\r\n";
        // constexpr char WakeCommand[] = "$PMTK010,002*2D\r\n";
    }
    TinyGPSPlus* GetGps()
    {
        return &Gps;
    }
    void Begin()
    {
        LOG_TRACE( "Gps::Begin()" );
        uart_config_t uart_config;
        uart_config.baud_rate = 9600;
        uart_config.data_bits = UART_DATA_8_BITS;
        uart_config.parity = UART_PARITY_DISABLE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_config.rx_flow_ctrl_thresh = 120;

        uart_param_config( GpsUartPort, &uart_config );

        uart_set_pin( GpsUartPort,
                      Pin::GpsRx,         // TX
                      Pin::GpsTx,         // RX
                      UART_PIN_NO_CHANGE, // RTS
                      UART_PIN_NO_CHANGE  // CTS
        );

        uart_driver_install( GpsUartPort, 2048, 2048, 10, &GpsUartQueue, 0 );
    }
    void Service()
    {
        size_t count = 0;
        uint8_t data[ 128 ];
        uart_get_buffered_data_len( GpsUartPort, &count );
        if( count > 0 )
        {
            while( count > 0 )
            {
                auto read = uart_read_bytes( GpsUartPort, data, min( count, sizeof( data ) ), 100 );
                for( int i = 0; i < read; ++i )
                {
                    Gps.encode( static_cast<char>( data[ i ] ) );
                }
                count -= read;
            }
        }

        if( Gps.location.isUpdated() )
        {
            float lat = Gps.location.lat();
            float lng = Gps.location.lng();
            // LOG_TRACE( "GPS: %f, %f", lat, lng );
        }
    }
    void Wake()
    {
        // I never managed to get low power mode working on the new SAM-M8Q.
        // auto bytes_pushed = uart_write_bytes( GpsUartPort, WakeCommand, sizeof( WakeCommand ) - 1 );
        // uart_wait_tx_done( GpsUartPort, 100 );
    }
    void Sleep()
    {
        // I never managed to get low power mode working on the new SAM-M8Q.
        // auto bytes_pushed = uart_write_bytes( GpsUartPort, StandbyCommand, sizeof( StandbyCommand ) - 1 );
        // uart_wait_tx_done( GpsUartPort, 100 );
    }
}