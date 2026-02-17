#include "timeline_config.h"
#include "FreeRTOS.h"
#include "task.h"

static void vBusyWaitMs(uint32_t ulDurationMs)
{
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);
    TickType_t xWorkedTicks = 0U;
    TickType_t xLastObservedTick = xTaskGetTickCount();

    while (xWorkedTicks < xTargetTicks) {
        TickType_t xNowTick = xTaskGetTickCount();
        TickType_t xDeltaTicks = xNowTick - xLastObservedTick;

        if (xDeltaTicks == 1U) {
            xWorkedTicks++;
            xLastObservedTick = xNowTick;
        } else if (xDeltaTicks > 1U) {
            xLastObservedTick = xNowTick;
        }
    }
}

static uint32_t ulGetTaskRuntimeMs(const void * pvArg, uint32_t ulFallbackMs)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;

    if ((pxExecInfo == NULL) ||
        (pxExecInfo->ulEndOffsetMs <= pxExecInfo->ulStartOffsetMs)) {
        return ulFallbackMs;
    }

    return pxExecInfo->ulRunDurationMs;
}

static void vHrtSenseTask(void * pvArg)
{
    vBusyWaitMs(ulGetTaskRuntimeMs(pvArg, 0U));
}

static void vHrtControlTask(void * pvArg)
{
    vBusyWaitMs(ulGetTaskRuntimeMs(pvArg, 0U));
}

static void vHrtActuationTask(void * pvArg)
{
    vBusyWaitMs(ulGetTaskRuntimeMs(pvArg, 0U));
}

static void vSrtLongTask(void * pvArg)
{
    vBusyWaitMs(ulGetTaskRuntimeMs(pvArg, 0U));
}

static void vSrtLoggerTask(void * pvArg)
{
    vBusyWaitMs(ulGetTaskRuntimeMs(pvArg, 0U));
}

static const TimelineTaskConfig_t xTasks[] = {
    /* HRT runtime = ulEndOffsetMs - ulStartOffsetMs. */
    { "HRT_A",     vHrtSenseTask,     TIMELINE_TASK_HRT, 0, 5, 15, tskIDLE_PRIORITY + 4, 256 },
    { "HRT_B",     vHrtControlTask,   TIMELINE_TASK_HRT, 1, 5, 20, tskIDLE_PRIORITY + 4, 256 },
    { "HRT_test", vHrtControlTask,   TIMELINE_TASK_HRT, 1, 15, 25, tskIDLE_PRIORITY + 4, 256 },
    { "HRT_C",     vHrtActuationTask, TIMELINE_TASK_HRT, 3, 5, 15, tskIDLE_PRIORITY + 4, 256 },

    /* SRT fixed compile-time order. Timing fields are placeholders for SRT in current scheduler. */
    { "SRT_LONG",  vSrtLongTask,      TIMELINE_TASK_SRT, 0, 0, 10, tskIDLE_PRIORITY + 1, 256 },
    { "SRT_LOG",   vSrtLoggerTask,    TIMELINE_TASK_SRT, 0, 10, 20, tskIDLE_PRIORITY + 1, 256 }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100,
    .ulSubframeMs = 25,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
