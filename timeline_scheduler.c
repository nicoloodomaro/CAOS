#include "timeline_scheduler.h"

#include "timers.h"

#ifndef TIMELINE_ASSERT
#define TIMELINE_ASSERT(config) configASSERT(config)
#endif

typedef struct TimelineTaskContext {
    UBaseType_t uxIndex;
} TimelineTaskContext_t;

typedef struct TimelineSchedulerState {
    const TimelineConfig_t * pxConfig;
    TickType_t xMajorFrameTicks;
    TickType_t xSubframeTicks;
    TickType_t xFrameStartTick;
    BaseType_t xStarted;
    BaseType_t xConfigValid;
    BaseType_t xFrameResetPending;
    TickType_t xLastTickSeen;
    TimelineTaskContext_t xTaskContexts[TIMELINE_MAX_TASKS];
    TimelineTaskRuntime_t xRuntime[TIMELINE_MAX_TASKS];
} TimelineSchedulerState_t;

static TimelineSchedulerState_t xTimeline;

static void prvTimelineManagedTask(void * pvArg)
{
    const TimelineTaskContext_t * pxCtx = (const TimelineTaskContext_t *) pvArg;
    const UBaseType_t uxIndex = pxCtx->uxIndex;

    for (;;) {
        (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if ((xTimeline.pxConfig == NULL) || (uxIndex >= xTimeline.pxConfig->ulTaskCount)) {
            vTaskSuspend(NULL);
            continue;
        }

        xTimeline.pxConfig->pxTasks[uxIndex].pxTaskCode(NULL);
        vTimelineSchedulerTaskCompletedFromTaskContext(uxIndex);
        vTaskSuspend(NULL);
    }
}

static BaseType_t prvValidateConfig(const TimelineConfig_t * pxConfig)
{
    uint32_t ulIdx;

    if ((pxConfig == NULL) || (pxConfig->pxTasks == NULL) || (pxConfig->ulTaskCount == 0U)) {
        return pdFALSE;
    }

    if ((pxConfig->ulTaskCount > TIMELINE_MAX_TASKS) || (pxConfig->ulSubframeMs == 0U) ||
        (pxConfig->ulMajorFrameMs == 0U) || ((pxConfig->ulMajorFrameMs % pxConfig->ulSubframeMs) != 0U)) {
        return pdFALSE;
    }

    for (ulIdx = 0U; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &pxConfig->pxTasks[ulIdx];
        if ((pxTask->pcName == NULL) || (pxTask->pxTaskCode == NULL)) {
            return pdFALSE;
        }

        if (pxTask->xType == TIMELINE_TASK_HRT) {
            if ((pxTask->ulEndOffsetMs <= pxTask->ulStartOffsetMs) ||
                (pxTask->ulEndOffsetMs > pxConfig->ulSubframeMs)) {
                return pdFALSE;
            }
        }
    }

    return pdTRUE;
}

static void prvResetFrameRuntimeState(void)
{
    uint32_t ulIdx;

    TIMELINE_ASSERT(xTimeline.pxConfig != NULL);
    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        xTimeline.xRuntime[ulIdx].xIsActive = pdFALSE;
        xTimeline.xRuntime[ulIdx].xCompletedInWindow = pdFALSE;
        xTimeline.xRuntime[ulIdx].xDeadlineMissPendingKill = pdFALSE;
    }
}

static BaseType_t prvCreateManagedTaskIfMissing(UBaseType_t uxIndex)
{
    TaskHandle_t xHandle = NULL;
    const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[uxIndex];

    if (xTimeline.xRuntime[uxIndex].xHandle != NULL) {
        return pdPASS;
    }

    xTimeline.xTaskContexts[uxIndex].uxIndex = uxIndex;
    if (xTaskCreate(prvTimelineManagedTask,
                    pxTask->pcName,
                    pxTask->usStackDepthWords,
                    &xTimeline.xTaskContexts[uxIndex],
                    pxTask->uxPriority,
                    &xHandle) != pdPASS) {
        return pdFAIL;
    }

    xTimeline.xRuntime[uxIndex].xHandle = xHandle;
    xTimeline.xRuntime[uxIndex].xIsActive = pdFALSE;
    xTimeline.xRuntime[uxIndex].xCompletedInWindow = pdFALSE;
    xTimeline.xRuntime[uxIndex].xDeadlineMissPendingKill = pdFALSE;

    vTaskSuspend(xHandle);
    return pdPASS;
}

static void prvProcessPendingKillsAndRecreate(void)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if ((xTimeline.xRuntime[ulIdx].xDeadlineMissPendingKill != pdFALSE) &&
            (xTimeline.xRuntime[ulIdx].xHandle != NULL)) {
            vTaskDelete(xTimeline.xRuntime[ulIdx].xHandle);
            xTimeline.xRuntime[ulIdx].xHandle = NULL;
            xTimeline.xRuntime[ulIdx].xDeadlineMissPendingKill = pdFALSE;
            xTimeline.xRuntime[ulIdx].xIsActive = pdFALSE;
        }

        if (xTimeline.xRuntime[ulIdx].xHandle == NULL) {
            (void) prvCreateManagedTaskIfMissing((UBaseType_t) ulIdx);
        }
    }
}

BaseType_t xTimelineSchedulerConfigure(const TimelineConfig_t * pxConfig)
{
    uint32_t ulIdx;

    if (prvValidateConfig(pxConfig) == pdFALSE) {
        return pdFAIL;
    }

    xTimeline.pxConfig = pxConfig;
    xTimeline.xMajorFrameTicks = pdMS_TO_TICKS(pxConfig->ulMajorFrameMs);
    xTimeline.xSubframeTicks = pdMS_TO_TICKS(pxConfig->ulSubframeMs);
    xTimeline.xStarted = pdFALSE;
    xTimeline.xConfigValid = pdTRUE;
    xTimeline.xFrameResetPending = pdFALSE;
    xTimeline.xLastTickSeen = 0;

    for (ulIdx = 0U; ulIdx < TIMELINE_MAX_TASKS; ulIdx++) {
        xTimeline.xRuntime[ulIdx].xHandle = NULL;
        xTimeline.xRuntime[ulIdx].ulReleaseCount = 0U;
        xTimeline.xRuntime[ulIdx].ulCompletionCount = 0U;
        xTimeline.xRuntime[ulIdx].ulDeadlineMissCount = 0U;
        xTimeline.xRuntime[ulIdx].xIsActive = pdFALSE;
        xTimeline.xRuntime[ulIdx].xCompletedInWindow = pdFALSE;
        xTimeline.xRuntime[ulIdx].xDeadlineMissPendingKill = pdFALSE;
    }

    return pdPASS;
}

BaseType_t xTimelineSchedulerCreateManagedTasks(void)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return pdFAIL;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if (prvCreateManagedTaskIfMissing((UBaseType_t) ulIdx) != pdPASS) {
            return pdFAIL;
        }
    }

    return pdPASS;
}

void vTimelineSchedulerKernelStart(TickType_t xStartTick)
{
    TIMELINE_ASSERT(xTimeline.xConfigValid == pdTRUE);
    xTimeline.xFrameStartTick = xStartTick;
    xTimeline.xLastTickSeen = xStartTick;
    xTimeline.xStarted = pdTRUE;
    prvResetFrameRuntimeState();
}

void vTimelineSchedulerOnTickFromISR(TickType_t xNowTick, BaseType_t * pxHigherPriorityTaskWoken)
{
    uint32_t ulIdx;
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;

    if ((xTimeline.xStarted == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    xTimeline.xLastTickSeen = xNowTick;
    xTicksFromFrame = xNowTick - xTimeline.xFrameStartTick;

    if (xTicksFromFrame >= xTimeline.xMajorFrameTicks) {
        xTimeline.xFrameStartTick = xNowTick;
        xTimeline.xFrameResetPending = pdTRUE;
        xTicksFromFrame = 0;
        prvResetFrameRuntimeState();
    }

    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
    xTickInSubframe = xTicksFromFrame % xTimeline.xSubframeTicks;

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == ulCurrentSubframe) &&
            (xTickInSubframe == pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
            (pxRt->xHandle != NULL)) {
            pxRt->xIsActive = pdTRUE;
            pxRt->xCompletedInWindow = pdFALSE;
            pxRt->ulReleaseCount++;
            vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);
            (void) xTaskResumeFromISR(pxRt->xHandle);
        }

        if ((pxTask->xType == TIMELINE_TASK_HRT) && (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulEndOffsetMs))) {
            pxRt->xDeadlineMissPendingKill = pdTRUE;
            pxRt->ulDeadlineMissCount++;
            pxRt->xIsActive = pdFALSE;
        }

        if ((pxTask->xType == TIMELINE_TASK_SRT) && (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive == pdFALSE)) {
            BaseType_t xNoHrtActive = pdTRUE;
            uint32_t ulProbe;

            for (ulProbe = 0U; ulProbe < xTimeline.pxConfig->ulTaskCount; ulProbe++) {
                if ((xTimeline.pxConfig->pxTasks[ulProbe].xType == TIMELINE_TASK_HRT) &&
                    (xTimeline.xRuntime[ulProbe].xIsActive != pdFALSE)) {
                    xNoHrtActive = pdFALSE;
                    break;
                }
            }

            if (xNoHrtActive != pdFALSE) {
                pxRt->xIsActive = pdTRUE;
                pxRt->ulReleaseCount++;
                vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);
                (void) xTaskResumeFromISR(pxRt->xHandle);
            }
        }
    }
}

TaskHandle_t xTimelineSchedulerSelectNextTask(TaskHandle_t xDefaultSelected, TickType_t xNowTick)
{
    uint32_t ulIdx;
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;

    (void) xNowTick;

    if ((xTimeline.xStarted == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return xDefaultSelected;
    }

    prvProcessPendingKillsAndRecreate();

    if (xTimeline.xFrameResetPending != pdFALSE) {
        xTimeline.xFrameResetPending = pdFALSE;
    }

    xTicksFromFrame = xTimeline.xLastTickSeen - xTimeline.xFrameStartTick;
    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
    xTickInSubframe = xTicksFromFrame % xTimeline.xSubframeTicks;

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == ulCurrentSubframe) &&
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
            (xTickInSubframe < pdMS_TO_TICKS(pxTask->ulEndOffsetMs)) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            return pxRt->xHandle;
        }
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_SRT) && (pxRt->xHandle != NULL) &&
            (pxRt->xCompletedInWindow == pdFALSE)) {
            return pxRt->xHandle;
        }
    }

    return xDefaultSelected;
}

void vTimelineSchedulerTaskCompletedFromTaskContext(UBaseType_t uxTaskIndex)
{
    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return;
    }

    taskENTER_CRITICAL();
    xTimeline.xRuntime[uxTaskIndex].xCompletedInWindow = pdTRUE;
    xTimeline.xRuntime[uxTaskIndex].xIsActive = pdFALSE;
    xTimeline.xRuntime[uxTaskIndex].ulCompletionCount++;
    taskEXIT_CRITICAL();
}

const TimelineTaskRuntime_t * pxTimelineSchedulerGetRuntime(uint32_t * pulTaskCount)
{
    if ((pulTaskCount != NULL) && (xTimeline.pxConfig != NULL)) {
        *pulTaskCount = xTimeline.pxConfig->ulTaskCount;
    }

    return &xTimeline.xRuntime[0];
}
