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

static void vHrtATask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vHrtBTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;

    vBusyWaitMs(pxExecInfo, 20U);
}

static void vHrtCTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vSrtXTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static void vSrtYTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vSrtZTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}



/* Profile 4: deterministic schedule for timing consistency checks. */

/*
a 0 parte SRT_X che runna fino a 2 (gli mancano 4 tick per finire)
a 2 parte HRT_A che runna fino a 9 e termina
a 9 si passa a SRT_X che runna fino a 13 e termina
a 13 si passa a SRT_Y che runna fino a 16 e termina
a 16 si passa a SRT_z che runna fino a 26 e termina
a 26 si passa a IDLE per 1 tick perchè a 27 arriva HRT_B
a 27 parte HRT_B che runna fino a 47 e termina
a 47 si passa a IDLE fino a 54 perchè arriva HRT_C
a 54 parte HRT_C che runna fino a 58 e termina
a 58 si passa a IDLE fino a 81 perchè arriva HRT_D
a 81 parte HRT_D che runna fino a 88 e termina
a 88 si passa a IDLE fino a 100


*/
static const TimelineTaskConfig_t xTasksTimingConsistency[] = {
    { "HRT_A", vHrtATask, TIMELINE_TASK_HRT, 0U, 2U, 12U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtBTask, TIMELINE_TASK_HRT, 1U, 2U, 24U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtCTask, TIMELINE_TASK_HRT, 2U, 4U, 10U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtATask, TIMELINE_TASK_HRT, 3U, 6U, 16U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_X", vSrtXTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Y", vSrtYTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Z", vSrtZTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasksTimingConsistency,
    .ulTaskCount = (uint32_t) (sizeof(xTasksTimingConsistency) / sizeof(xTasksTimingConsistency[0]))
};