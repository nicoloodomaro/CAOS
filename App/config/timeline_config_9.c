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

static void vHrtChain1(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 9U);
}

static void vHrtChain2(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 23U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 130U);
}

static void vSrtShort(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static const TimelineTaskConfig_t xTasks[] = {
    /*
     * Error profile with same frame geometry as config_7/config_11:
     * - chained HRT misses in same subframe
     * - HRT crossing subframe boundary
     * - HRT carry-over at major-frame boundary
     * - SRT carry-over at major-frame boundary
     */
    { "HRT_CH1",      vHrtChain1,      TIMELINE_TASK_HRT, 0U, 0U, 6U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_CH2",      vHrtChain2,      TIMELINE_TASK_HRT, 0U, 6U, 14U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 2U, 2U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 14U, 20U, tskIDLE_PRIORITY + 4U, 256U },

    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_SHORT",    vSrtShort,       TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
