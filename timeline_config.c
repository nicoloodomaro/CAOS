#include "timeline_config.h"

static void vHrtSensorTask(void * pvArg)
{
    (void) pvArg;
}

static void vHrtControlTask(void * pvArg)
{
    (void) pvArg;
}

static void vSrtLoggerTask(void * pvArg)
{
    (void) pvArg;
}

static void vSrtDiagTask(void * pvArg)
{
    (void) pvArg;
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_SENSE", vHrtSensorTask, TIMELINE_TASK_HRT, 0U, 0U, 2U, tskIDLE_PRIORITY + 3U, 256U },
    { "HRT_CTRL",  vHrtControlTask, TIMELINE_TASK_HRT, 1U, 1U, 4U, tskIDLE_PRIORITY + 3U, 256U },
    { "SRT_LOG",   vSrtLoggerTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_DIAG",  vSrtDiagTask, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 10U,
    .ulSubframeMs = 5U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};
