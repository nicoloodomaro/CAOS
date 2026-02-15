#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

void UART_init( void );
void UART_putc( char c );
void UART_puts( const char * s );
void UART_put_u32( uint32_t value );
void UART_put_task_event( const char * task_name,
                          char event_code );

#endif /* UART_H */
