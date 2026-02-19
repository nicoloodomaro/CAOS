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
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vSrtXTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 70U);
}

static void vSrtYTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}


/* Profile 3: repeated SRT->HRT preemption across all subframes. */


/*
parte SRT_X a 0 e runna fino a 5 (gli mancano 65 tick per finire)
a 5 c'è ctx switch da SRT_X a HRT_A, HRT_A runna da 5 a 12 e termina
a 12 riparte SRT_X fino a 27 (gli mancano 50 tick per finire)
a 27 c'è ctx switch da SRT_X a HRT_B, HRT_B runna da 27 a 31 e termina
a 31 riparte SRT_X fino a 53 (gli mancano 28 tick per finire)
a 53 parte HRT_C che runna fino a 60 e termina
a 60 riparte SRT_X fino a 76 (gli mancano 12 tick per finire)
a 76 parte HRT_D che runna fino a 80 e termina
a 80 riparte SRT_X fino a 92 e termina
a 92 parte SRT_Y che runna fino a 95 e termina
a 95 è già idle fino a 100

*/
static const TimelineTaskConfig_t xTasksPreemptionChain[] = {
    { "HRT_A", vHrtATask, TIMELINE_TASK_HRT, 0U, 5U, 14U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtBTask, TIMELINE_TASK_HRT, 1U, 2U, 8U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtATask, TIMELINE_TASK_HRT, 2U, 3U, 12U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtBTask, TIMELINE_TASK_HRT, 3U, 1U, 6U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_X", vSrtXTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Y", vSrtYTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasksPreemptionChain,
    .ulTaskCount = (uint32_t) (sizeof(xTasksPreemptionChain) / sizeof(xTasksPreemptionChain[0]))
};