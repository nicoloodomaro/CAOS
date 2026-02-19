#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"

/* Auto-generated file. Do not edit manually. */
/* Source spec: /home/nico/Scrivania/progetto/CAOS/App/generated/timeline_problem.json */

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

static void vGeneratedTask_HRT(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 2U);
}

static void vGeneratedTask_SRT(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 15U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT", vGeneratedTask_HRT, TIMELINE_TASK_HRT, 3U, 0U, 3U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT", vGeneratedTask_SRT, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 30U,
    .ulSubframeMs = 3U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
