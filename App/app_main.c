#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"
#include "uart.h"

int main(void)
{
#if ( DEBUG == 1 )
    UART_init();
#endif

    configASSERT(xTimelineSchedulerConfigure(&gTimelineConfig) == pdPASS);
    configASSERT(xTimelineSchedulerCreateManagedTasks() == pdPASS);

    vTaskStartScheduler();

    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char * pcTaskName)
{
    (void) xTask;
    (void) pcTaskName;

    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vAssertCalled(const char * pcFileName, uint32_t ulLine)
{
    (void) pcFileName;
    (void) ulLine;

    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
