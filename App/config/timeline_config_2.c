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


static void vHrtCTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}


static void vSrtXTask(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}




/* Profile 2: Min gap between HRT 

parte HRT_A a 0 e runna fino a 4.
a 4 si passa da HRT_A a SRT_Y fino a 6, quando arriva HRT_B 
a 6 arriva HRT_B, che runna fino a 10, poi SRT_Y fino a 11
a 11 si passa da SRT_Y a idle fino a 12
a 12 arriva HRT_C, che runna fino a 16, poi idle fino a 18
a 18 arriva HRT_D e inizia a runnare. 
a 22 HRT_D termina, si passa a idle.  
al subframe 1 a 46 si passa a HRT_E che runna fino a 49
a 50 si passa a idle fino alla fine del subframe

*/

static const TimelineTaskConfig_t xTasksEdgeMinGap[] = {
    { "HRT_A", vHrtCTask, TIMELINE_TASK_HRT, 0U, 0U, 5U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtCTask, TIMELINE_TASK_HRT, 0U, 6U, 11U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtCTask, TIMELINE_TASK_HRT, 0U, 12U, 17U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtCTask, TIMELINE_TASK_HRT, 0U, 18U, 23U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_E", vHrtCTask, TIMELINE_TASK_HRT, 1U, 21U, 25U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_X", vSrtXTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};


const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 100U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasksEdgeMinGap,
    .ulTaskCount = (uint32_t) (sizeof(xTasksEdgeMinGap) / sizeof(xTasksEdgeMinGap[0]))
};