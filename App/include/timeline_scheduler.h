#ifndef TIMELINE_SCHEDULER_H
#define TIMELINE_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#define TIMELINE_MAX_TASKS    32

typedef enum TimelineTaskType {
    TIMELINE_TASK_HRT = 0,
    TIMELINE_TASK_SRT = 1
} TimelineTaskType_t;

typedef struct TimelineTaskConfig {
    const char * pcName;
    TaskFunction_t pxTaskCode;
    TimelineTaskType_t xType;
    uint32_t ulSubframeId;
    uint32_t ulStartOffsetMs;
    uint32_t ulEndOffsetMs;
    UBaseType_t uxPriority;
    uint16_t usStackDepthWords;
} TimelineTaskConfig_t;

typedef struct TimelineTaskExecutionInfo {
    UBaseType_t uxTaskIndex;
    uint32_t ulSubframeId;
    uint32_t ulStartOffsetMs;
    uint32_t ulEndOffsetMs;
    uint32_t ulRunDurationMs;
} TimelineTaskExecutionInfo_t;

typedef struct TimelineConfig {
    uint32_t ulMajorFrameMs;
    uint32_t ulSubframeMs;
    const TimelineTaskConfig_t * pxTasks;
    uint32_t ulTaskCount;
} TimelineConfig_t;

typedef struct TimelineTaskRuntime {
    TaskHandle_t xHandle;
    uint32_t ulReleaseCount;
    uint32_t ulCompletionCount;
    uint32_t ulDeadlineMissCount;
    BaseType_t xIsActive;
    BaseType_t xCompletedInWindow;
    BaseType_t xDeadlineMissPendingKill;
} TimelineTaskRuntime_t;

typedef enum TimelineTraceEventType {
    TIMELINE_TRACE_EVT_FRAME_START = 0,
    TIMELINE_TRACE_EVT_HRT_RELEASE = 1,
    TIMELINE_TRACE_EVT_SRT_RELEASE = 2,
    TIMELINE_TRACE_EVT_TASK_COMPLETE = 3,
    TIMELINE_TRACE_EVT_DEADLINE_MISS = 4,
    TIMELINE_TRACE_EVT_HRT_ENQUEUED = 5
} TimelineTraceEventType_t;

typedef struct TimelineTraceEvent {
    TickType_t xTick;
    uint32_t ulFrameId;
    uint32_t ulSubframeId;
    UBaseType_t uxTaskIndex;
    TimelineTraceEventType_t xType;
} TimelineTraceEvent_t;

BaseType_t xTimelineSchedulerConfigure(const TimelineConfig_t * pxConfig);
BaseType_t xTimelineSchedulerCreateManagedTasks(void);
BaseType_t xTimelineSchedulerIsConfigured(void);
void vTimelineSchedulerSetDebugEnabled(BaseType_t xEnable);
void vTimelineSchedulerKernelStart(TickType_t xStartTick);
void vTimelineSchedulerOnTickFromISR(TickType_t xNowTick, BaseType_t * pxHigherPriorityTaskWoken);
TaskHandle_t xTimelineSchedulerSelectNextTask(TaskHandle_t xDefaultSelected, TickType_t xNowTick);
void vTimelineSchedulerTaskCompletedFromTaskContext(UBaseType_t uxTaskIndex);
const TimelineTaskRuntime_t * pxTimelineSchedulerGetRuntime(uint32_t * pulTaskCount);
uint32_t ulTimelineSchedulerTraceRead(TimelineTraceEvent_t * pxBuffer, uint32_t ulMaxEvents);


#endif
