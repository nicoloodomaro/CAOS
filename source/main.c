#include "timeline_scheduler.h"
#include "timeline_config.h"
#include "utils.h"

void vApplicationDeadlineMissedHook(const char *pcTaskName) {
    UART_printf("\n[ERROR] Deadline Missed: ");
    UART_printf(pcTaskName);
    UART_printf("\n");
}


// Chiamata se un task usa troppa memoria dello stack
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    UART_printf("\n[FATAL] Stack Overflow: ");
    UART_printf(pcTaskName);
    while(1); // Blocca tutto
}

// Chiamata se malloc fallisce (memoria finita)
void vApplicationMallocFailedHook(void) {
    UART_printf("\n[FATAL] Malloc Failed! Heap full.\n");
    while(1);
}

int main(void) {
    // 1. Setup Hardware (specifico per MPS2/QEMU, es. UART print)
    UART_init();
    UART_printf("\n--- MPS2 Timeline Scheduler Starting ---\n");

    // 2. Recupera la configurazione della Timeline
    const TimelineConfig_t *cfg = get_system_configuration();

    // 3. Avvia lo scheduler custom (che avvierà FreeRTOS)
    vStartTimelineScheduler(cfg);

    // 4. Non dovremmo mai arrivare qui
    return 0;
}