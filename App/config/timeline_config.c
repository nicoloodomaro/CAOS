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

static void vSrtWTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static void vSrtXTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vSrtYTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}


static void vSrtZTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 70U);
}



/*
Parte SRT_W a 0 e runna fino a 5 (gli manca ancora 1 tick per finire)
a 5 arriva HRT_A e inizia a runnare 
a 10 arriva HRT_B ma viene messo in coda perchè HRT_A è in esecuzione
a 12 HRT_A termina, e parte HRT_B che runna fino a 20 e va in deadline miss
a 20 riparte SRT_W che runna fino a 21 e termina
a 21 parte SRT_X che runna fino a 24 e termina
a 24 arriva SRT_Y che runna fino a 34 e termina
a 34 parte SRT_Z che runna fino a 80 (gli mancano 25 tick per finire)
a 80 parte HRT_c che runna fino a 84 e termina
a 84 riparte SRT_Z che runna fino a 100 ma non riesce a terminare (deadline miss)


*/
static const TimelineTaskConfig_t xTasks[] = {
    
    { "HRT_A",     vHrtATask,     TIMELINE_TASK_HRT, 0U, 5U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B",     vHrtBTask,   TIMELINE_TASK_HRT, 0U, 10U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C",     vHrtCTask, TIMELINE_TASK_HRT, 3U, 5U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_W",  vSrtWTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_X",   vSrtXTask,    TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Y",  vSrtYTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_Z",  vSrtZTask,      TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
