#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// Registri della UART per MPS2 (CMSDK UART)
#define UART0_ADDRESS   ( 0x40004000UL )
#define UART0_DATA      ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 0UL ) ) ) )
#define UART0_STATE     ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) ) )
#define UART0_CTRL      ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) ) )
#define UART0_BAUDDIV   ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) ) )

// Inizializza la UART
void UART_init(void);

// Stampa una stringa
void UART_printf(const char *s);

// Stampa un numero (utile per debuggare i tempi!)
void UART_print_int(uint32_t val);

#endif /* UTILS_H */