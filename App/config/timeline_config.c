#include "timeline_config.h"
#include "FreeRTOS.h"
#include "task.h"

static void vBusyWaitMs(uint32_t ulDurationMs)
{
    TickType_t xStartTick = xTaskGetTickCount();
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);

    while ((xTaskGetTickCount() - xStartTick) < xTargetTicks) {
    }
}

static void vHrtSenseTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(1U);
}

static void vHrtControlTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(1U);
}

static void vHrtActuationTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(4U);
}

static void vSrtLongTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(6U);
}

static void vSrtLoggerTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(1U);
}

static void vSrtDiagTask(void * pvArg)
{
    (void) pvArg;
    vBusyWaitMs(10U);
}

static const TimelineTaskConfig_t xTasks[] = {
    /*
     * TEST profile with two visible cases:
     * 1) Preemption SRT->HRT:
     *    - sf0: HRT_A release at t=5ms (window 5..15ms), preempting SRT work.
     *    - sf1: HRT_B release at t=5ms in subframe 1 (window 5..20ms).
     * 2) HRT deadline miss:
     *    - In this current configuration HRT_C does NOT miss deadline
     *      (window 5..15ms, body ~4ms). To force a miss, shrink end offset
     *      or increase task execution time.
     */
    { "HRT_A",     vHrtSenseTask,     TIMELINE_TASK_HRT, 0U, 5U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B",     vHrtControlTask,   TIMELINE_TASK_HRT, 1U, 5U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C",     vHrtActuationTask, TIMELINE_TASK_HRT, 3U, 5U, 15U, tskIDLE_PRIORITY + 4U, 256U },

    /* SRT fixed compile-time order. Timing fields are placeholders for SRT in current scheduler. */
    { "SRT_LONG",  vSrtLongTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_LOG",   vSrtLoggerTask,    TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_DIAG",  vSrtDiagTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
    //{ "SRT_DIAG",  vSrtDiagTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};