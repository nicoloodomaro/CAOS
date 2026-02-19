#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"

static void vBusyWaitMs(const TimelineTaskExecutionInfo_t * pxExecInfo, uint32_t ulDurationMs)
{
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);

    if ((pxExecInfo == NULL) || (xTargetTicks == 0U)) {
        return;
    }

    while (xTimelineSchedulerGetTaskExecutedTicks(*pxExecInfo) < xTargetTicks) {
        taskYIELD();
    }
}

static void vHrtOtto(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vHrtDodici(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vHrtSette(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vHrtVenti(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 20U);
}

static void vHrtTre(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vSrtTrenta(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 30U);
}

static void vSrtSette(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vSrtQuarantacinque(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 45U);
}

static void vSrtDiciotto(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 18U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_A", vHrtTre,    TIMELINE_TASK_HRT, 0U, 3U, 15U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtDodici, TIMELINE_TASK_HRT, 0U, 7U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtSette,  TIMELINE_TASK_HRT, 1U, 5U, 15U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtVenti,  TIMELINE_TASK_HRT, 3U, 0U, 12U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_E", vHrtOtto,   TIMELINE_TASK_HRT, 7U, 11U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_F", vHrtTre,    TIMELINE_TASK_HRT, 9U, 12U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_A", vSrtTrenta, TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_B", vSrtSette,  TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_C", vSrtQuarantacinque, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_D", vSrtDiciotto,       TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 200U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
