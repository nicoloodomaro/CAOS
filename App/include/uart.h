#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

#ifndef DEBUG
#define DEBUG    1
#endif

#define UART0_ADDRESS     ( 0x40004000UL )
#define UART0_DATA        ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 0UL ) ) )
#define UART0_STATE       ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) )
#define UART0_CTRL        ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) )
#define UART0_BAUDDIV     ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) )
#define UART_TX_BUSY_MASK ( 1UL )

void UART_init( void );
void UART_putc( char c );
void UART_puts( const char * s );
void UART_put_u32( uint32_t value );
void UART_put_task_event( const char * task_name,
                          char event_code );

#endif /* UART_H */
