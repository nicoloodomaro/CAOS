#include "timeline_config.h"
#include "FreeRTOS.h"
#include "task.h"

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

static void vHrtSenseTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vHrtControlTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 20U);
}

static void vHrtActuationTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vSrtLongTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static void vSrtLoggerTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vSrtDiagTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}


static void vSrtPorcTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 70U);
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
    { "HRT_B",     vHrtControlTask,   TIMELINE_TASK_HRT, 0U, 10U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    //{ "HRT_test2",     vHrtActuationTask, TIMELINE_TASK_HRT, 0U, 23U, 25U, tskIDLE_PRIORITY + 4U, 256U },
    //{ "HRT_test",     vHrtActuationTask, TIMELINE_TASK_HRT, 1U, 0U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C",     vHrtActuationTask, TIMELINE_TASK_HRT, 3U, 5U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    //{ "HRT_D",     vHrtActuationTask, TIMELINE_TASK_HRT, 3U, 8U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    //{ "HRT_E",     vHrtActuationTask, TIMELINE_TASK_HRT, 3U, 12U, 20U, tskIDLE_PRIORITY + 4U, 256U },


    /* SRT fixed compile-time order. Timing fields are placeholders for SRT in current scheduler. */
    { "SRT_LONG",  vSrtLongTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_LOG",   vSrtLoggerTask,    TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_DIAG",  vSrtDiagTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_PORC",  vSrtPorcTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },

    //{ "SRT_DIAG",  vSrtDiagTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
