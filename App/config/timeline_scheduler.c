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
    BaseType_t xMaintenanceRequestPending;
    TickType_t xLastTickSeen;
    uint32_t ulFrameId;
    TaskHandle_t xLastSelectedHandle;
    TimelineTaskContext_t xTaskContexts[TIMELINE_MAX_TASKS];
    TimelineTaskRuntime_t xRuntime[TIMELINE_MAX_TASKS];
} TimelineSchedulerState_t;

static TimelineSchedulerState_t xTimeline;

#ifndef TIMELINE_TRACE_BUFFER_LEN
#define TIMELINE_TRACE_BUFFER_LEN    256U
#endif

typedef struct TimelineTraceBufferState {
    TimelineTraceEvent_t xEvents[TIMELINE_TRACE_BUFFER_LEN];
    uint32_t ulHead;
    uint32_t ulTail;
} TimelineTraceBufferState_t;

static TimelineTraceBufferState_t xTrace;

static uint32_t prvComputeCurrentSubframe(void)
{
    TickType_t xTicksFromFrame;

    if ((xTimeline.pxConfig == NULL) || (xTimeline.xSubframeTicks == 0U)) {
        return 0U;
    }

    xTicksFromFrame = xTimeline.xLastTickSeen - xTimeline.xFrameStartTick;
    return (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
}

static void prvTracePush(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    uint32_t ulNextHead;

    xTrace.xEvents[xTrace.ulHead].xTick = xTimeline.xLastTickSeen;
    xTrace.xEvents[xTrace.ulHead].ulFrameId = xTimeline.ulFrameId;
    xTrace.xEvents[xTrace.ulHead].ulSubframeId = ulSubframeId;
    xTrace.xEvents[xTrace.ulHead].uxTaskIndex = uxTaskIndex;
    xTrace.xEvents[xTrace.ulHead].xType = xType;

    ulNextHead = (xTrace.ulHead + 1U) % TIMELINE_TRACE_BUFFER_LEN;
    xTrace.ulHead = ulNextHead;

    if (xTrace.ulHead == xTrace.ulTail) {
        xTrace.ulTail = (xTrace.ulTail + 1U) % TIMELINE_TRACE_BUFFER_LEN;
    }
}

static void prvTracePushFromTask(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    taskENTER_CRITICAL();
    prvTracePush(xType, uxTaskIndex, ulSubframeId);
    taskEXIT_CRITICAL();
}

static void prvTracePushFromISR(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    UBaseType_t uxSavedInterruptStatus;

    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    prvTracePush(xType, uxTaskIndex, ulSubframeId);
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

/* Called from kernel task-selection path where caller already holds the
 * scheduler/interrupt protection needed for consistent trace writes. */
static void prvTracePushFromSchedulerContext(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    prvTracePush(xType, uxTaskIndex, ulSubframeId);
}

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
    TickType_t xMajorFrameTicks;
    TickType_t xSubframeTicks;
    uint32_t ulSubframeCount;

    if ((pxConfig == NULL) || (pxConfig->pxTasks == NULL) || (pxConfig->ulTaskCount == 0U)) {
        return pdFALSE;
    }

    if ((pxConfig->ulTaskCount > TIMELINE_MAX_TASKS) || (pxConfig->ulSubframeMs == 0U) ||
        (pxConfig->ulMajorFrameMs == 0U) || ((pxConfig->ulMajorFrameMs % pxConfig->ulSubframeMs) != 0U)) {
        return pdFALSE;
    }

    xMajorFrameTicks = pdMS_TO_TICKS(pxConfig->ulMajorFrameMs);
    xSubframeTicks = pdMS_TO_TICKS(pxConfig->ulSubframeMs);
    if ((xMajorFrameTicks == 0U) || (xSubframeTicks == 0U) ||
        ((xMajorFrameTicks % xSubframeTicks) != 0U)) {
        return pdFALSE;
    }

    ulSubframeCount = (uint32_t) (xMajorFrameTicks / xSubframeTicks);
    for (ulIdx = 0U; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &pxConfig->pxTasks[ulIdx];
        if ((pxTask->pcName == NULL) || (pxTask->pxTaskCode == NULL)) {
            return pdFALSE;
        }

        if (pxTask->xType == TIMELINE_TASK_HRT) {
            const TickType_t xStartTick = pdMS_TO_TICKS(pxTask->ulStartOffsetMs);
            const TickType_t xEndTick = pdMS_TO_TICKS(pxTask->ulEndOffsetMs);

            if (pxTask->ulSubframeId >= ulSubframeCount) {
                return pdFALSE;
            }

            if ((xEndTick <= xStartTick) || (xEndTick > xSubframeTicks)) {
                return pdFALSE;
            }
        }
    }

    for (ulIdx = 0U; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTaskA = &pxConfig->pxTasks[ulIdx];
        uint32_t ulProbe;

        if (pxTaskA->xType != TIMELINE_TASK_HRT) {
            continue;
        }

        for (ulProbe = ulIdx + 1U; ulProbe < pxConfig->ulTaskCount; ulProbe++) {
            const TimelineTaskConfig_t * pxTaskB = &pxConfig->pxTasks[ulProbe];

            if (pxTaskB->xType != TIMELINE_TASK_HRT) {
                continue;
            }

            if (pxTaskA->ulSubframeId == pxTaskB->ulSubframeId) {
                const TickType_t xAStart = pdMS_TO_TICKS(pxTaskA->ulStartOffsetMs);
                const TickType_t xAEnd = pdMS_TO_TICKS(pxTaskA->ulEndOffsetMs);
                const TickType_t xBStart = pdMS_TO_TICKS(pxTaskB->ulStartOffsetMs);
                const TickType_t xBEnd = pdMS_TO_TICKS(pxTaskB->ulEndOffsetMs);

                if ((xAStart < xBEnd) && (xBStart < xAEnd)) {
                    return pdFALSE;
                }
            }
        }
    }

    /* SRT tasks must remain compact and ordered after all HRT entries. */
    {
        BaseType_t xFoundFirstSrt = pdFALSE;
        BaseType_t xFoundHrtAfterSrt = pdFALSE;

        for (ulIdx = 0U; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &pxConfig->pxTasks[ulIdx];

            if (pxTask->xType == TIMELINE_TASK_SRT) {
                xFoundFirstSrt = pdTRUE;
            } else if ((pxTask->xType == TIMELINE_TASK_HRT) && (xFoundFirstSrt != pdFALSE)) {
                xFoundHrtAfterSrt = pdTRUE;
            }
        }

        if (xFoundHrtAfterSrt != pdFALSE) {
            return pdFALSE;
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

BaseType_t xTimelineSchedulerIsConfigured(void)
{
    if ((xTimeline.xConfigValid != pdFALSE) && (xTimeline.pxConfig != NULL)) {
        return pdTRUE;
    }

    return pdFALSE;
}

uint32_t ulTimelineSchedulerTraceRead(TimelineTraceEvent_t * pxBuffer, uint32_t ulMaxEvents)
{
    uint32_t ulReadCount = 0U;

    if ((pxBuffer == NULL) || (ulMaxEvents == 0U)) {
        return 0U;
    }

    taskENTER_CRITICAL();
    while ((xTrace.ulTail != xTrace.ulHead) && (ulReadCount < ulMaxEvents)) {
        pxBuffer[ulReadCount] = xTrace.xEvents[xTrace.ulTail];
        xTrace.ulTail = (xTrace.ulTail + 1U) % TIMELINE_TRACE_BUFFER_LEN;
        ulReadCount++;
    }
    taskEXIT_CRITICAL();

    return ulReadCount;
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

static BaseType_t prvAnyPendingMaintenanceFromISR(void)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return pdFALSE;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if ((xTimeline.xRuntime[ulIdx].xDeadlineMissPendingKill != pdFALSE) &&
            (xTimeline.xRuntime[ulIdx].xHandle != NULL)) {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static void prvMarkFrameBoundaryCarryOverFromISR(uint32_t ulCurrentSubframe)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxRt->xHandle == NULL) ||
            (pxRt->xIsActive == pdFALSE) ||
            (pxRt->xCompletedInWindow != pdFALSE) ||
            (pxRt->xDeadlineMissPendingKill != pdFALSE)) {
            continue;
        }

        pxRt->xDeadlineMissPendingKill = pdTRUE;
        pxRt->xIsActive = pdFALSE;

        if (pxTask->xType == TIMELINE_TASK_HRT) {
            pxRt->ulDeadlineMissCount++;
            prvTracePushFromISR(TIMELINE_TRACE_EVT_DEADLINE_MISS, (UBaseType_t) ulIdx, ulCurrentSubframe);
        }
    }
}

static void prvTimelineMaintenanceCallback(void * pvUnusedParam1, uint32_t ulUnusedParam2)
{
    (void) pvUnusedParam1;
    (void) ulUnusedParam2;

    xTimeline.xMaintenanceRequestPending = pdFALSE;
    prvProcessPendingKillsAndRecreate();
}

static void prvReleaseFrameStartTasksFromTaskContext(void)
{
    uint32_t ulIdx;
    BaseType_t xAnyHrtActive = pdFALSE;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    /* The first frame starts exactly at xFrameStartTick, so tasks configured
     * at subframe 0 / offset 0 must be released immediately. */
    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == 0U) &&
            (pdMS_TO_TICKS(pxTask->ulStartOffsetMs) == 0U) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            pxRt->xIsActive = pdTRUE;
            pxRt->xCompletedInWindow = pdFALSE;
            pxRt->ulReleaseCount++;
            xTaskNotifyGive(pxRt->xHandle);
            vTaskResume(pxRt->xHandle);
            prvTracePushFromTask(TIMELINE_TRACE_EVT_HRT_RELEASE, (UBaseType_t) ulIdx, 0U);
            xAnyHrtActive = pdTRUE;
        }
    }

    /* If no HRT is active at frame start, release first SRT immediately. */
    if (xAnyHrtActive == pdFALSE) {
        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if ((pxTask->xType == TIMELINE_TASK_SRT) &&
                (pxRt->xHandle != NULL) &&
                (pxRt->xIsActive == pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE)) {
                pxRt->xIsActive = pdTRUE;
                pxRt->ulReleaseCount++;
                xTaskNotifyGive(pxRt->xHandle);
                vTaskResume(pxRt->xHandle);
                prvTracePushFromTask(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, 0U);
                break;
            }
        }
    }
}

static BaseType_t prvFindTaskIndexByHandle(TaskHandle_t xHandle, UBaseType_t * puxIndex)
{
    uint32_t ulIdx;

    if ((xTimeline.pxConfig == NULL) || (xHandle == NULL) || (puxIndex == NULL)) {
        return pdFALSE;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if (xTimeline.xRuntime[ulIdx].xHandle == xHandle) {
            *puxIndex = (UBaseType_t) ulIdx;
            return pdTRUE;
        }
    }

    return pdFALSE;
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
    xTimeline.xMaintenanceRequestPending = pdFALSE;
    xTimeline.xLastTickSeen = 0;
    xTimeline.ulFrameId = 0U;
    xTimeline.xLastSelectedHandle = NULL;
    xTrace.ulHead = 0U;
    xTrace.ulTail = 0U;

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
    xTimeline.xMaintenanceRequestPending = pdFALSE;
    xTimeline.ulFrameId = 0U;
    xTimeline.xLastSelectedHandle = NULL;
    prvResetFrameRuntimeState();
    prvTracePushFromTask(TIMELINE_TRACE_EVT_FRAME_START, 0U, 0U);
    prvReleaseFrameStartTasksFromTaskContext();
}

void vTimelineSchedulerOnTickFromISR(TickType_t xNowTick, BaseType_t * pxHigherPriorityTaskWoken)
{
    uint32_t ulIdx;
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;
    BaseType_t xSrtReleasedThisTick = pdFALSE;

    if ((xTimeline.xStarted == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    xTimeline.xLastTickSeen = xNowTick;
    xTicksFromFrame = xNowTick - xTimeline.xFrameStartTick;
    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);

    if (xTicksFromFrame >= xTimeline.xMajorFrameTicks) {
        prvMarkFrameBoundaryCarryOverFromISR(ulCurrentSubframe);
        xTimeline.xFrameStartTick = xNowTick;
        xTimeline.xFrameResetPending = pdTRUE;
        xTicksFromFrame = 0U;
        xTimeline.ulFrameId++;
        prvResetFrameRuntimeState();
        prvTracePushFromISR(TIMELINE_TRACE_EVT_FRAME_START, 0U, 0U);
    }

    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
    xTickInSubframe = xTicksFromFrame % xTimeline.xSubframeTicks;

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == ulCurrentSubframe) &&
            (xTickInSubframe == pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            BaseType_t xResumeHigherPriorityTaskWoken = pdFALSE;

            pxRt->xIsActive = pdTRUE;
            pxRt->xCompletedInWindow = pdFALSE;
            pxRt->ulReleaseCount++;
            vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);
            xResumeHigherPriorityTaskWoken = xTaskResumeFromISR(pxRt->xHandle);
            if ((pxHigherPriorityTaskWoken != NULL) && (xResumeHigherPriorityTaskWoken != pdFALSE)) {
                *pxHigherPriorityTaskWoken = pdTRUE;
            }
            /* HRT release should preempt promptly at the same tick boundary. */
            if (pxHigherPriorityTaskWoken != NULL) {
                *pxHigherPriorityTaskWoken = pdTRUE;
            }
            prvTracePushFromISR(TIMELINE_TRACE_EVT_HRT_RELEASE, (UBaseType_t) ulIdx, ulCurrentSubframe);
        }

        if ((pxTask->xType == TIMELINE_TASK_HRT) && (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
            ((ulCurrentSubframe != pxTask->ulSubframeId) ||
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulEndOffsetMs)))) { // > e non >= per consentire completamento al tick di deadline
            pxRt->xDeadlineMissPendingKill = pdTRUE;
            pxRt->ulDeadlineMissCount++;
            pxRt->xIsActive = pdFALSE;
            prvTracePushFromISR(TIMELINE_TRACE_EVT_DEADLINE_MISS, (UBaseType_t) ulIdx, ulCurrentSubframe);
        }

        if ((pxTask->xType == TIMELINE_TASK_SRT) && (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            BaseType_t xNoHrtActive = pdTRUE;
            BaseType_t xIsFirstIncompleteSrt = pdTRUE;
            uint32_t ulProbe;

            for (ulProbe = 0U; ulProbe < xTimeline.pxConfig->ulTaskCount; ulProbe++) {
                if ((xTimeline.pxConfig->pxTasks[ulProbe].xType == TIMELINE_TASK_HRT) &&
                    (xTimeline.xRuntime[ulProbe].xIsActive != pdFALSE)) {
                    xNoHrtActive = pdFALSE;
                    break;
                }
            }

            /* Keep SRT ordering strict: only the first not-yet-completed SRT
             * in compile-time order can be released from tick context. */
            for (ulProbe = 0U; ulProbe < ulIdx; ulProbe++) {
                if ((xTimeline.pxConfig->pxTasks[ulProbe].xType == TIMELINE_TASK_SRT) &&
                    (xTimeline.xRuntime[ulProbe].xDeadlineMissPendingKill == pdFALSE) &&
                    (xTimeline.xRuntime[ulProbe].xCompletedInWindow == pdFALSE)) {
                    xIsFirstIncompleteSrt = pdFALSE;
                    break;
                }
            }

            /* Release only the first eligible SRT in compile-time order
             * to enforce fixed ordering. Subsequent SRTs are released
             * when the preceding one completes. */
            if ((xNoHrtActive != pdFALSE) && (xSrtReleasedThisTick == pdFALSE) &&
                (xIsFirstIncompleteSrt != pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE)) {
                BaseType_t xResumeHigherPriorityTaskWoken = pdFALSE;

                pxRt->xIsActive = pdTRUE;
                pxRt->ulReleaseCount++;
                vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);
                xResumeHigherPriorityTaskWoken = xTaskResumeFromISR(pxRt->xHandle);
                if ((pxHigherPriorityTaskWoken != NULL) && (xResumeHigherPriorityTaskWoken != pdFALSE)) {
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                xSrtReleasedThisTick = pdTRUE;
                prvTracePushFromISR(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, ulCurrentSubframe);
            }
        }
    }

    if ((xTimeline.xMaintenanceRequestPending == pdFALSE) &&
        (prvAnyPendingMaintenanceFromISR() != pdFALSE)) {
        BaseType_t xTimerTaskWoken = pdFALSE;

        if (xTimerPendFunctionCallFromISR(prvTimelineMaintenanceCallback, NULL, 0U, &xTimerTaskWoken) == pdPASS) {
            xTimeline.xMaintenanceRequestPending = pdTRUE;

            if ((pxHigherPriorityTaskWoken != NULL) && (xTimerTaskWoken != pdFALSE)) {
                *pxHigherPriorityTaskWoken = pdTRUE;
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
    TaskHandle_t xSelected = xDefaultSelected;

    (void) xNowTick;

    if ((xTimeline.xStarted == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return xDefaultSelected;
    }

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
            (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            xSelected = pxRt->xHandle;
            break;
        }
    }

    if (xSelected == xDefaultSelected) {
        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if ((pxTask->xType == TIMELINE_TASK_SRT) && (pxRt->xHandle != NULL) &&
                (pxRt->xIsActive != pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE)) {
                xSelected = pxRt->xHandle;
                break;
            }
        }
    }

    if (xSelected != xTimeline.xLastSelectedHandle) {
        UBaseType_t uxSelectedIdx;
        UBaseType_t uxPrevSelectedIdx;

        if (prvFindTaskIndexByHandle(xSelected, &uxSelectedIdx) != pdFALSE) {
            prvTracePushFromSchedulerContext(TIMELINE_TRACE_EVT_CONTEXT_SWITCH, uxSelectedIdx, ulCurrentSubframe);
        } else if (prvFindTaskIndexByHandle(xTimeline.xLastSelectedHandle, &uxPrevSelectedIdx) != pdFALSE) {
            /* Trace switch away from timeline-managed task to a non-managed one. */
            (void) uxPrevSelectedIdx;
            prvTracePushFromSchedulerContext(TIMELINE_TRACE_EVT_CONTEXT_SWITCH, (UBaseType_t) TIMELINE_MAX_TASKS, ulCurrentSubframe);
        }

        xTimeline.xLastSelectedHandle = xSelected;
    }

    return xSelected;
}

void vTimelineSchedulerTaskCompletedFromTaskContext(UBaseType_t uxTaskIndex)
{
    BaseType_t xCanCountCompletion = pdFALSE;

    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return;
    }

    taskENTER_CRITICAL();
    if (xTimeline.xRuntime[uxTaskIndex].xDeadlineMissPendingKill == pdFALSE) {
        xTimeline.xRuntime[uxTaskIndex].xCompletedInWindow = pdTRUE;
        xTimeline.xRuntime[uxTaskIndex].ulCompletionCount++;
        xCanCountCompletion = pdTRUE;
    }

    xTimeline.xRuntime[uxTaskIndex].xIsActive = pdFALSE;
    taskEXIT_CRITICAL();

    if (xCanCountCompletion == pdFALSE) {
        return;
    }

    prvTracePushFromTask(TIMELINE_TRACE_EVT_TASK_COMPLETE, uxTaskIndex, prvComputeCurrentSubframe());

    /* Immediately release the next SRT in fixed order (if any) so SRTs
     * execute sequentially within the available idle time of the subframe. */
    {
        BaseType_t xNoHrtActive = pdTRUE;
        uint32_t ulIdx;

        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            if ((xTimeline.pxConfig->pxTasks[ulIdx].xType == TIMELINE_TASK_HRT) &&
                (xTimeline.xRuntime[ulIdx].xIsActive != pdFALSE)) {
                xNoHrtActive = pdFALSE;
                break;
            }
        }

        if (xNoHrtActive == pdFALSE) {
            return;
        }

        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if ((pxTask->xType == TIMELINE_TASK_SRT) && (pxRt->xHandle != NULL) &&
                (pxRt->xIsActive == pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE)) {
                /* Mark active and release from task context. */
                taskENTER_CRITICAL();
                pxRt->xIsActive = pdTRUE;
                pxRt->ulReleaseCount++;
                taskEXIT_CRITICAL();

                xTaskNotifyGive(pxRt->xHandle);
                vTaskResume(pxRt->xHandle);
                prvTracePushFromTask(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, prvComputeCurrentSubframe());
                break; /* release only the next SRT */
            }
        }
    }
}

const TimelineTaskRuntime_t * pxTimelineSchedulerGetRuntime(uint32_t * pulTaskCount)
{
    if ((pulTaskCount != NULL) && (xTimeline.pxConfig != NULL)) {
        *pulTaskCount = xTimeline.pxConfig->ulTaskCount;
    }

    return &xTimeline.xRuntime[0];
}
