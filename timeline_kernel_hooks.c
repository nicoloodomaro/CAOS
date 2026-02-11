#include "timeline_kernel_hooks.h"
#include "timeline_scheduler.h"

void vTimelineKernelHookSchedulerStart(void)
{
    vTimelineSchedulerKernelStart(xTaskGetTickCount());
}

void vTimelineKernelHookTick(TickType_t xTickCount, BaseType_t * pxHigherPriorityTaskWoken)
{
    vTimelineSchedulerOnTickFromISR(xTickCount, pxHigherPriorityTaskWoken);
}

TaskHandle_t xTimelineKernelHookSelectTask(TaskHandle_t xDefaultSelected, TickType_t xTickCount)
{
    return xTimelineSchedulerSelectNextTask(xDefaultSelected, xTickCount);
}
