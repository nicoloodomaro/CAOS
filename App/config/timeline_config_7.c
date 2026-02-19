#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"

static void vBusyWaitMs(const TimelineTaskExecutionInfo_t * pxExecInfo, uint32_t ulDurationMs)
{
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);

    if ((pxExecInfo == NULL) || (xTargetTicks == 0U)) {
        return;
    }

    while (xTimelineSchedulerGetTaskExecutedTicks(pxExecInfo->uxTaskIndex) < xTargetTicks) {
        taskYIELD();
    }
}

static void vHrtMissA(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 24U);
}

static void vHrtMissB(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 140U);
}

static void vSrtShort(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 5U);
}

static const TimelineTaskConfig_t xTasks[] = {
    /*
     * Error profile (all runtime-reachable):
     * - HRT_MISS_A: classic HRT deadline miss inside subframe.
     * - HRT_PASS_SF: active HRT crosses subframe boundary, then miss.
     * - HRT_MISS_B: released but blocked by another HRT, then miss.
     * - HRT_MAJ_MISS: released in last subframe and killed at major-frame boundary.
     * - SRT_MAJ_MISS: long SRT carries over major-frame boundary.
     */
    { "HRT_MISS_A",   vHrtMissA,       TIMELINE_TASK_HRT, 0U, 1U, 8U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 1U, 0U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MISS_B",   vHrtMissB,       TIMELINE_TASK_HRT, 1U, 5U, 12U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 15U, 20U, tskIDLE_PRIORITY + 4U, 256U },

    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_SHORT",    vSrtShort,       TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
