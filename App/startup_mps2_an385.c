/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include "uart.h"

#include <stdint.h>

/* FreeRTOS interrupt handlers. */
extern void vPortSVCHandler( void );
extern void xPortPendSVHandler( void );
extern void xPortSysTickHandler( void );

/* Exception handlers. */
static void HardFault_Handler( void ) __attribute__( ( naked ) );
static void Default_Handler( void ) __attribute__( ( naked ) );
void Reset_Handler( void ) __attribute__( ( naked ) );

extern int main( void );
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

/* Keep C runtime init in a C helper while preserving professor-style Reset_Handler. */
__attribute__( ( used ) ) void prvStartupInitAndRun( void );

/* Vector table. */
const uint32_t * isr_vector[] __attribute__( ( section( ".isr_vector" ), used ) ) =
{
    ( uint32_t * ) &_estack,
    ( uint32_t * ) &Reset_Handler,      /* Reset                 -15 */
    ( uint32_t * ) &Default_Handler,    /* NMI_Handler           -14 */
    ( uint32_t * ) &HardFault_Handler,  /* HardFault_Handler     -13 */
    ( uint32_t * ) &Default_Handler,    /* MemManage_Handler     -12 */
    ( uint32_t * ) &Default_Handler,    /* BusFault_Handler      -11 */
    ( uint32_t * ) &Default_Handler,    /* UsageFault_Handler    -10 */
    0,                                  /* reserved               -9 */
    0,                                  /* reserved               -8 */
    0,                                  /* reserved               -7 */
    0,                                  /* reserved               -6 */
    ( uint32_t * ) &vPortSVCHandler,    /* SVC_Handler            -5 */
    ( uint32_t * ) &Default_Handler,    /* DebugMon_Handler       -4 */
    0,                                  /* reserved               -3 */
    ( uint32_t * ) &xPortPendSVHandler, /* PendSV handler         -2 */
    ( uint32_t * ) &xPortSysTickHandler,/* SysTick_Handler        -1 */
    0,                                  /* uart0 receive           0 */
    0,                                  /* uart0 transmit          1 */
    0,                                  /* uart1 receive           2 */
    0,                                  /* uart1 transmit          3 */
    0,                                  /* uart2 receive           4 */
    0,                                  /* uart2 transmit          5 */
    0,                                  /* GPIO 0 combined IRQ     6 */
    0,                                  /* GPIO 2 combined IRQ     7 */
    0,                                  /* Timer 0                 8 */
    0,                                  /* Timer 1                 9 */
    0,                                  /* Dual timer             10 */
    0,                                  /* SPI0 SPI1              11 */
    0,                                  /* uart overflow 1,2,3    12 */
    0                                   /* Ethernet               13 */
};

void Reset_Handler( void )
{
    __asm volatile
    (
        "ldr r0, =_estack               \n"
        "mov sp, r0                     \n"
        "bl  prvStartupInitAndRun       \n"
        "b   .                          \n"
    );
}

void prvStartupInitAndRun( void )
{
    uint32_t * pulSrc;
    uint32_t * pulDst;

    /* Copy .data section from flash to RAM. */
    pulSrc = &_sidata;
    for( pulDst = &_sdata; pulDst < &_edata; pulDst++ )
    {
        *pulDst = *pulSrc;
        pulSrc++;
    }

    /* Zero out .bss section. */
    for( pulDst = &_sbss; pulDst < &_ebss; pulDst++ )
    {
        *pulDst = 0U;
    }

    ( void ) main();

    for( ; ; )
    {
    }
}

/* Variables used to store the value of registers at the time a hardfault
 * occurs. These are volatile to try and prevent compiler optimization.
 */
volatile uint32_t r0;
volatile uint32_t r1;
volatile uint32_t r2;
volatile uint32_t r3;
volatile uint32_t r12;
volatile uint32_t lr;  /* Link register. */
volatile uint32_t pc;  /* Program counter. */
volatile uint32_t psr; /* Program status register. */

/* Called from hardfault handler to provide processor state at fault time. */
__attribute__( ( used ) ) void prvGetRegistersFromStack( uint32_t * pulFaultStackAddress )
{
    r0 = pulFaultStackAddress[ 0 ];
    r1 = pulFaultStackAddress[ 1 ];
    r2 = pulFaultStackAddress[ 2 ];
    r3 = pulFaultStackAddress[ 3 ];

    r12 = pulFaultStackAddress[ 4 ];
    lr = pulFaultStackAddress[ 5 ];
    pc = pulFaultStackAddress[ 6 ];
    psr = pulFaultStackAddress[ 7 ];

    UART_puts( "Calling prvGetRegistersFromStack() from fault handler\r\n" );

    /* When the following loop is hit, variables contain register values. */
    for( ; ; )
    {
    }
}

void Default_Handler( void )
{
    __asm volatile
    (
        ".align 8                                \n"
        " ldr r3, =0xe000ed04                    \n"
        " ldr r2, [r3, #0]                       \n"
        " uxtb r2, r2                            \n"
        "Infinite_Loop:                          \n"
        " b  Infinite_Loop                       \n"
        " .ltorg                                 \n"
    );
}

void HardFault_Handler( void )
{
    __asm volatile
    (
        ".align 8                                                   \n"
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, =prvGetRegistersFromStack                         \n"
        " bx r2                                                     \n"
        " .ltorg                                                    \n"
    );
}
