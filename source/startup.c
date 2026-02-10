#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _etext, _sdata, _edata, _sbss, _ebss;

extern int main(void);
extern void SystemInit(void); 

void Reset_Handler(void);
void Default_Handler(void) { while (1); }

// FreeRTOSConfig.h mappa le funzioni di FreeRTOS su questi nomi
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

__attribute__((section(".isr_vector")))
void (*const g_pfnVectors[])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    Default_Handler, // NMI
    Default_Handler, // Hard Fault
    Default_Handler, // MemManage
    Default_Handler, // BusFault
    Default_Handler, // UsageFault
    0, 0, 0, 0,
    SVC_Handler,     // <--- NOME CORRETTO
    Default_Handler,
    0,
    PendSV_Handler,  // <--- NOME CORRETTO
    SysTick_Handler  // <--- NOME CORRETTO
};

void Reset_Handler(void) {
    uint32_t *pSrc, *pDest;
    pSrc = &_etext;
    pDest = &_sdata;
    while (pDest < &_edata) *pDest++ = *pSrc++;
    pDest = &_sbss;
    while (pDest < &_ebss) *pDest++ = 0;
    main();
}

void SystemInit(void) {}