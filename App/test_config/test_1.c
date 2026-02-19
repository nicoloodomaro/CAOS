#include "timeline_config.h"
#include "FreeRTOS.h"
#include "task.h"

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

static void vHrtATask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 20U);
}

static void vHrtBTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
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




/* Profile 1: stress with multiple overlapping HRT tasks in the same subframe. */

// il primo HRT inizia finchè non termina, gli altri HRT tendono a overlapparsi fino a scadere
/*
parte HRT_A a 0 e runna.
HRT_B arriva a 2 ma viene messo in coda perchè HRT_A è in esecuzione, HRT_B aspetta.
HRT_C arriva a 4, ma HRT_A è ancora in esecuzione, HRT_B è in coda, quindi anche HRT_C viene messo in coda
a 9 HRT_C scade quindi va in deadline miss, HRT_B è ancora in coda
a 11 HRT_B scade, anche lui deadline miss
a 20 HRT_A termina, parte SRT_X che runna fino a 25
a 25 finisce il subframe 0 e parte HRT_D   (ctx switch da SRT_X a HRT_D)
HRT_D runna da 25 a 33, poi va in deadline miss
a 26 arriva HRT_E, ma HRT_D è in esecuzione, quindi HRT_E va in coda
a 31 HRT_E scade, deadline miss
a 33 HRT_D termina (deadline miss), si ritorna a SRT_X che runna ancora per 1 tick fino a 34
a 34 part SRT_Y che runna fino a 37
dopo 37 tutto idle fino a 100
*/

static const TimelineTaskConfig_t xTasksStressOverlapHrt[] = {
    { "HRT_A", vHrtATask, TIMELINE_TASK_HRT, 0U, 0U, 23U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtBTask, TIMELINE_TASK_HRT, 0U, 2U, 11U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtCTask, TIMELINE_TASK_HRT, 0U, 4U, 9U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtATask, TIMELINE_TASK_HRT, 1U, 0U, 8U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_E", vHrtCTask, TIMELINE_TASK_HRT, 1U, 1U, 6U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_X", vSrtXTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Y", vSrtYTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasksStressOverlapHrt,
    .ulTaskCount = (uint32_t) (sizeof(xTasksStressOverlapHrt) / sizeof(xTasksStressOverlapHrt[0]))
};
