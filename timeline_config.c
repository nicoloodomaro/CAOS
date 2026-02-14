#include "timeline_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

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
    printf("[TASK] HRT_A start tick=%u\r\n", (unsigned int) xTaskGetTickCount());
    vBusyWaitMs(1U);
    printf("[TASK] HRT_A end   tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static void vHrtControlTask(void * pvArg)
{
    (void) pvArg;
    printf("[TASK] HRT_B start tick=%u\r\n", (unsigned int) xTaskGetTickCount());
    vBusyWaitMs(1U);
    printf("[TASK] HRT_B end   tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static void vHrtActuationTask(void * pvArg)
{
    
    (void) pvArg;
    printf("[TASK] HRT_C(start, will miss) tick=%u\r\n", (unsigned int) xTaskGetTickCount());
    vBusyWaitMs(4U);
    printf("[TASK] HRT_C end(unexpected if killed) tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static void vSrtLongTask(void * pvArg)
{
    TickType_t xStart;
    TickType_t xLastPrint;

    (void) pvArg;
    xStart = xTaskGetTickCount();
    xLastPrint = xStart;
    printf("[TASK] SRT_LONG start tick=%u\r\n", (unsigned int) xStart);

    while ((xTaskGetTickCount() - xStart) < pdMS_TO_TICKS(6U)) {
        TickType_t xNow = xTaskGetTickCount();
        if (xNow != xLastPrint) {
            xLastPrint = xNow;
            printf("[TASK] SRT_LONG run   tick=%u\r\n", (unsigned int) xNow);
        }
    }

    printf("[TASK] SRT_LONG end   tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static void vSrtLoggerTask(void * pvArg)
{
    (void) pvArg;
    printf("[TASK] SRT_LOG  tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static void vSrtDiagTask(void * pvArg)
{
    (void) pvArg;
    printf("[TASK] SRT_DIAG tick=%u\r\n", (unsigned int) xTaskGetTickCount());
}

static const TimelineTaskConfig_t xTasks[] = {
    /*
     * TEST profile with two visible cases:
     * 1) Preemption SRT->HRT:
     *    - sf0: HRT_A at t=0..1ms, then SRT_LONG starts.
     *    - sf0: HRT_B at t=3..4ms preempts SRT_LONG.
     * 2) HRT deadline miss:
     *    - sf1: HRT_C window t=1..3ms, but task body takes 4ms.
     *      It should be flagged as deadline miss and killed/recreated.
     */
    { "HRT_A",     vHrtSenseTask,     TIMELINE_TASK_HRT, 0U, 0U, 1U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B",     vHrtControlTask,   TIMELINE_TASK_HRT, 0U, 3U, 4U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C",     vHrtActuationTask, TIMELINE_TASK_HRT, 1U, 1U, 3U, tskIDLE_PRIORITY + 4U, 256U },

    /* SRT fixed compile-time order. Timing fields are placeholders for SRT in current scheduler. */
    { "SRT_LONG",  vSrtLongTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_LOG",   vSrtLoggerTask,    TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_DIAG",  vSrtDiagTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 10U,
    .ulSubframeMs = 5U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
