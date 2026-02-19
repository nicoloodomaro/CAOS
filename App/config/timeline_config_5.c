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
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vSrtYTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 20U);
}


/* Profile 5: intentional HRT misses to validate kill/recreate consistency. */

/*
parte SRT_X a 0 che runna fino a 3 (ha bisogno di 3 tick per finire)
a 3 parte HRT_A che runna fino a 15 e fa deadline miss (perchè doveva eseguire per 20 tick)
a 15 parte SRT_X che runna fino a 18 e termina
a 18 parte SRT_Y che runna fino a 35 (perchè arriva HRT_B, ha bisogno di 4 tick per terminare)
a 35 parte HRT_B che runna fino a 39 e fa deadline miss (perchè doveva eseguire per 7 tick)
a 39 riparte SRT_Y che runna fino a 43 e termina
a 43 si passa a idle fino a 55
a 55 arriva HRT_C che runna fino a 59 e termina
a 59 si passa a idle fino a 75
a 75 arriva HRT_D che runna fino a 82 e termina
a 82 si passa a idle fino a 100
*/

static const TimelineTaskConfig_t xTasksDeadlineMissRecovery[] = {
    { "HRT_A", vHrtBTask, TIMELINE_TASK_HRT, 0U, 3U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtATask, TIMELINE_TASK_HRT, 1U, 10U, 14U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtCTask, TIMELINE_TASK_HRT, 2U, 5U, 12U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtATask, TIMELINE_TASK_HRT, 3U, 0U, 10U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_X", vSrtXTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Y", vSrtYTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};



const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasksDeadlineMissRecovery,
    .ulTaskCount = (uint32_t) (sizeof(xTasksDeadlineMissRecovery) / sizeof(xTasksDeadlineMissRecovery[0]))
};