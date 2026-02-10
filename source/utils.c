#include "utils.h"
#include <sys/stat.h>

void UART_init( void )
{
    UART0_BAUDDIV = 16;
    UART0_CTRL = 1; // Abilita TX
}

void UART_printf(const char *s) {
    while(*s != '\0') {
        UART0_DATA = (unsigned int)(*s);
        s++;
    }
}

// Funzione helper veloce per stampare numeri senza usare librerie pesanti
void UART_print_int(uint32_t val) {
    char buffer[12];
    int i = 0;
    
    if (val == 0) {
        UART_printf("0");
        return;
    }

    while (val > 0) {
        buffer[i++] = (val % 10) + '0';
        val /= 10;
    }

    // Stampa al contrario
    while (--i >= 0) {
        UART0_DATA = (unsigned int)buffer[i];
    }
}

int _close(int file) { return -1; }
int _lseek(int file, int ptr, int dir) { return 0; }
int _read(int file, char *ptr, int len) { return 0; }
int _fstat(int file, struct stat *st) { st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { return 1; }

int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        UART0_DATA = (unsigned int)ptr[i];
    }
    return len;
}