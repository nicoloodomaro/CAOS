#include "FreeRTOSConfig.h"
#include "uart.h"
#include <stdint.h>


void UART_init( void )
{
    UART0_BAUDDIV = 16;
    UART0_CTRL = 1;
}

void UART_putc( char c )
{
    while( ( UART0_STATE & UART_TX_BUSY_MASK ) != 0 )
    {
    }

    UART0_DATA = ( uint32_t ) ( uint8_t ) c;
}

void UART_puts( const char * s )
{
    if( s == ( const char * ) 0 )
    {
        return;
    }

    while( *s != '\0' )
    {
        UART_putc( *s );
        s++;
    }
}

void UART_put_u32( uint32_t value )
{
    char digits[ 10 ];
    uint32_t idx = 0;

    if( value == 0 )
    {
        UART_putc( '0' );
        return;
    }

    while( value > 0 )
    {
        digits[ idx ] = ( char ) ( '0' + ( value % 10 ) );
        value /= 10;
        idx++;
    }

    while( idx > 0 )
    {
        idx--;
        UART_putc( digits[ idx ] );
    }
}

void UART_put_task_event( const char * task_name,
                          char event_code )
{
    UART_putc( ' ' );
    UART_puts( ( task_name != ( const char * ) 0 ) ? task_name : "-" );
    UART_putc( ':' );
    UART_putc( event_code );
}
