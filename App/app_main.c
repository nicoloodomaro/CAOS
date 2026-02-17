#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"
#include "uart.h"

#define DEBUG    1

int main(void)
{
    UART_init();

    configASSERT(xTimelineSchedulerConfigure(&gTimelineConfig) == pdPASS);
    configASSERT(xTimelineSchedulerCreateManagedTasks() == pdPASS);
    vTimelineSchedulerSetDebugEnabled((DEBUG == 1) ? pdTRUE : pdFALSE);

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

/* The startup vector references these IRQ handlers. */
void TIMER0_Handler(void)
{
}

void TIMER1_Handler(void)
{
}
