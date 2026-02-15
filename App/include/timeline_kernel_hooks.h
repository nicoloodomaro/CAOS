#ifndef TIMELINE_KERNEL_HOOKS_H
#define TIMELINE_KERNEL_HOOKS_H

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

void vTimelineKernelHookSchedulerStart(TickType_t xStartTick);
void vTimelineKernelHookTick(TickType_t xTickCount, BaseType_t * pxHigherPriorityTaskWoken);
TaskHandle_t xTimelineKernelHookSelectTask(TaskHandle_t xDefaultSelected, TickType_t xTickCount);

#ifdef __cplusplus
}
#endif

#endif
