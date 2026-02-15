#include "uart.h"
#include "FreeRTOSConfig.h"

#include <stdint.h>

#define UART0_ADDRESS     ( 0x40004000UL )
#define UART0_DATA        ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 0UL ) ) )
#define UART0_STATE       ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) )
#define UART0_CTRL        ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) )
#define UART0_BAUDDIV     ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) )
#define UART_TX_BUSY_MASK ( 1UL )

void UART_init( void )
{
    UART0_BAUDDIV = ( ( configCPU_CLOCK_HZ / 38400U ) - 1U );
    UART0_CTRL = 1U;
}

void UART_putc( char c )
{
    while( ( UART0_STATE & UART_TX_BUSY_MASK ) != 0U )
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
    uint32_t idx = 0U;

    if( value == 0U )
    {
        UART_putc( '0' );
        return;
    }

    while( value > 0U )
    {
        digits[ idx ] = ( char ) ( '0' + ( value % 10U ) );
        value /= 10U;
        idx++;
    }

    while( idx > 0U )
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
