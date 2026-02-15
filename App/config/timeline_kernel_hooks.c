#include "timeline_kernel_hooks.h"
#include "timeline_scheduler.h"
#include "timeline_config.h"

#ifndef TIMELINE_BOOTSTRAP_FROM_DEFAULT_CONFIG
#define TIMELINE_BOOTSTRAP_FROM_DEFAULT_CONFIG    1
#endif

void vTimelineKernelHookSchedulerStart(TickType_t xStartTick)
{
    if (xTimelineSchedulerIsConfigured() == pdFALSE) {
#if (TIMELINE_BOOTSTRAP_FROM_DEFAULT_CONFIG == 1)
        BaseType_t xCfgOk = xTimelineSchedulerConfigure(&gTimelineConfig);
        BaseType_t xCreateOk = pdFAIL;

        if (xCfgOk == pdPASS) {
            xCreateOk = xTimelineSchedulerCreateManagedTasks();
        }

        configASSERT(xCfgOk == pdPASS);
        configASSERT(xCreateOk == pdPASS);
#else
        configASSERT(!"Timeline scheduler not configured");
#endif
    }
    else {
        (void) xTimelineSchedulerCreateManagedTasks();
    }

    vTimelineSchedulerKernelStart(xStartTick);
}

void vTimelineKernelHookTick(TickType_t xTickCount, BaseType_t * pxHigherPriorityTaskWoken)
{
    vTimelineSchedulerOnTickFromISR(xTickCount, pxHigherPriorityTaskWoken);
}

TaskHandle_t xTimelineKernelHookSelectTask(TaskHandle_t xDefaultSelected, TickType_t xTickCount)
{
    return xTimelineSchedulerSelectNextTask(xDefaultSelected, xTickCount);
}
