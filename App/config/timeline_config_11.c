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

static void vHrtTightMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 11U);
}

static void vHrtBlockedMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 2U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 30U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 200U);
}

static void vSrtAux(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static const TimelineTaskConfig_t xTasks[] = {
    /*
     * Error profile with same frame geometry as config_7/config_9:
     * - tight-window HRT miss
     * - blocked HRT miss
     * - HRT crossing subframe boundary
     * - HRT and SRT carry-over at major-frame boundary
     */
    { "HRT_TIGHT",    vHrtTightMiss,   TIMELINE_TASK_HRT, 1U, 3U, 9U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_BLOCKED",  vHrtBlockedMiss, TIMELINE_TASK_HRT, 1U, 4U, 11U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 3U, 0U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 16U, 20U, tskIDLE_PRIORITY + 4U, 256U },

    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_AUX",      vSrtAux,         TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
