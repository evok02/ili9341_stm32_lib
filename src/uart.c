#include "uart.h"

#define LS_BAUD_RATE ( 9600 )
#define HS_BAUD_RATE ( 115200 )

void uart_setup( void ) {
    usart_set_mode( USART3, USART_MODE_TX_RX );
    usart_set_parity( USART3, USART_PARITY_NONE );
    usart_set_databits( USART3, 8 );
    usart_set_stopbits( USART3, USART_STOPBITS_1 );
    usart_set_flow_control( USART3, USART_FLOWCONTROL_NONE );
    usart_set_baudrate( USART3, HS_BAUD_RATE );

    usart_enable( USART3 );
}

void uart_write8( uint32_t uart_periph, uint8_t data ) {
    usart_send_blocking( uart_periph, ( uint16_t )data );
}
