#include "timeline_scheduler.h"
#include "timers.h"
#include "uart.h"

typedef struct TimelineTaskContext {
    UBaseType_t uxIndex;
    TimelineTaskExecutionInfo_t xExecInfo;
} TimelineTaskContext_t;

typedef struct TimelineSchedulerState {
    const TimelineConfig_t * pxConfig;
    TickType_t xMajorFrameTicks;
    TickType_t xSubframeTicks;
    TickType_t xFrameStartTick;
    BaseType_t xStarted;
    BaseType_t xDebugEnabled;
    BaseType_t xConfigValid;
    BaseType_t xFrameResetPending;
    BaseType_t xMaintenanceRequestPending;
    TickType_t xLastTickSeen;
    TaskHandle_t xLastSelectedHandle;
    uint32_t ulFrameId;
    TimelineTaskContext_t xTaskContexts[TIMELINE_MAX_TASKS];
    TimelineTaskRuntime_t xRuntime[TIMELINE_MAX_TASKS];
} TimelineSchedulerState_t;

static TimelineSchedulerState_t xTimeline;

#define TIMELINE_TRACE_BUFFER_LEN    256

typedef struct TimelineTraceBufferState {
    TimelineTraceEvent_t xEvents[TIMELINE_TRACE_BUFFER_LEN];
    uint32_t ulHead;
    uint32_t ulTail;
} TimelineTraceBufferState_t;

static TimelineTraceBufferState_t xTrace;
static BaseType_t xDebugTickOpen = pdFALSE;
static TickType_t xDebugOpenTick = 0;
static uint32_t ulDebugOpenFrameId = 0U;
static uint32_t ulDebugOpenSubframeId = 0U;
static TaskHandle_t xDebugOpenRunningHandle = NULL;

#define TIMELINE_DEBUG_CTX_EVENTS_MAX    8U

typedef struct TimelineDebugCtxEvent {
    TaskHandle_t xFrom;
    TaskHandle_t xTo;
} TimelineDebugCtxEvent_t;

static TimelineDebugCtxEvent_t xDebugOpenCtxEvents[TIMELINE_DEBUG_CTX_EVENTS_MAX];
static uint32_t ulDebugOpenCtxEventCount = 0U;

static void prvTracePushFromISR(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId);
static const char * prvTaskNameFromIndex(UBaseType_t uxTaskIndex);
static const char * prvTaskNameFromHandle(TaskHandle_t xHandle);
static TaskHandle_t prvSelectTimelineTask(TaskHandle_t xDefaultSelected, uint32_t ulCurrentSubframe, TickType_t xTickInSubframe);
static BaseType_t prvHasAnyActiveTaskOfType(TimelineTaskType_t xType);
static void prvDebugFinalizeOpenTickIfNeeded(TickType_t xNowTick);
static void prvDebugOpenTick(TickType_t xTick, uint32_t ulFrameId, uint32_t ulSubframeId);
static void prvDebugUpdateRunningTask(TaskHandle_t xSelectedHandle, uint32_t ulCurrentSubframe);
static void prvDebugPrintInstantTraceEvent(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId);
static void prvDebugPrintInstantCtxEvent(TaskHandle_t xFrom, TaskHandle_t xTo, uint32_t ulSubframeId);
static void prvReleaseTaskFromTaskContext(UBaseType_t uxTaskIndex);
static void prvReleaseTaskFromISR(UBaseType_t uxTaskIndex, BaseType_t * pxHigherPriorityTaskWoken);
static BaseType_t prvTryReleaseNextSrtFromTaskContext(uint32_t ulCurrentSubframe);
static BaseType_t prvTryReleaseNextSrtFromISR(uint32_t ulCurrentSubframe, BaseType_t * pxHigherPriorityTaskWoken);

static void prvDebugPrintTraceEvent(const TimelineTraceEvent_t * pxEvent)
{
    if (pxEvent == NULL) {
        return;
    }

    switch (pxEvent->xType) {
        case TIMELINE_TRACE_EVT_FRAME_START:
            UART_puts("frame-start");
            break;
        case TIMELINE_TRACE_EVT_HRT_RELEASE:
            UART_puts("release HRT ");
            UART_puts(prvTaskNameFromIndex(pxEvent->uxTaskIndex));
            break;
        case TIMELINE_TRACE_EVT_SRT_RELEASE:
            UART_puts("release SRT ");
            UART_puts(prvTaskNameFromIndex(pxEvent->uxTaskIndex));
            break;
        case TIMELINE_TRACE_EVT_TASK_COMPLETE:
            UART_puts("done ");
            UART_puts(prvTaskNameFromIndex(pxEvent->uxTaskIndex));
            break;
        case TIMELINE_TRACE_EVT_DEADLINE_MISS:
            UART_puts("interrupt ");
            UART_puts(prvTaskNameFromIndex(pxEvent->uxTaskIndex));
            UART_puts(" (deadline-miss)");
            break;
        default:
            UART_puts("event-unknown");
            break;
    }
}

static BaseType_t prvDebugPrintEventsForTick(TickType_t xTick,
                                             uint32_t ulFrameId,
                                             uint32_t ulSubframeId)
{
    BaseType_t xPrintedAny = pdFALSE;
    uint32_t ulIdx = xTrace.ulTail;

    while (ulIdx != xTrace.ulHead) {
        const TimelineTraceEvent_t * pxEvent = &xTrace.xEvents[ulIdx];
        if ((pxEvent->xTick == xTick) &&
            (pxEvent->ulFrameId == ulFrameId) &&
            (pxEvent->ulSubframeId == ulSubframeId)) {
            if (xPrintedAny != pdFALSE) {
                UART_puts("; ");
            }
            prvDebugPrintTraceEvent(pxEvent);
            xPrintedAny = pdTRUE;
        }
        ulIdx = (ulIdx + 1U) % TIMELINE_TRACE_BUFFER_LEN;
    }

    for (ulIdx = 0U; ulIdx < ulDebugOpenCtxEventCount; ulIdx++) {
        if (xPrintedAny != pdFALSE) {
            UART_puts("; ");
        }
        UART_puts("ctx ");
        UART_puts(prvTaskNameFromHandle(xDebugOpenCtxEvents[ulIdx].xFrom));
        UART_puts(" -> ");
        UART_puts(prvTaskNameFromHandle(xDebugOpenCtxEvents[ulIdx].xTo));
        xPrintedAny = pdTRUE;
    }

    return xPrintedAny;
}

static void prvDebugPrintTickSnapshot(TickType_t xTick,
                                      uint32_t ulFrameId,
                                      uint32_t ulSubframeId,
                                      TaskHandle_t xRunningHandle)
{
    if (xTimeline.xDebugEnabled == pdFALSE) {
        return;
    }

    UART_puts("tick ");
    UART_put_u32((uint32_t) xTick);
    UART_puts("\r\n");

    UART_puts("    major-frame = ");
    UART_put_u32(ulFrameId);
    UART_puts("\r\n");

    UART_puts("    subframe = ");
    UART_put_u32(ulSubframeId);
    UART_puts("\r\n");

    UART_puts("    events: ");
    if (prvDebugPrintEventsForTick(xTick, ulFrameId, ulSubframeId) == pdFALSE) {
        UART_puts("none");
    }
    UART_puts("\r\n");

    UART_puts("    running = ");
    UART_puts(prvTaskNameFromHandle(xRunningHandle));
    UART_puts("\r\n");
}

static void prvDebugOpenTick(TickType_t xTick, uint32_t ulFrameId, uint32_t ulSubframeId)
{
    xDebugTickOpen = pdTRUE;
    xDebugOpenTick = xTick;
    ulDebugOpenFrameId = ulFrameId;
    ulDebugOpenSubframeId = ulSubframeId;
    xDebugOpenRunningHandle = xTimeline.xLastSelectedHandle;
    ulDebugOpenCtxEventCount = 0U;
}

static void prvDebugFinalizeOpenTickIfNeeded(TickType_t xNowTick)
{
    if ((xTimeline.xDebugEnabled == pdFALSE) || (xDebugTickOpen == pdFALSE)) {
        return;
    }

    if (xNowTick == xDebugOpenTick) {
        return;
    }

    prvDebugPrintTickSnapshot(xDebugOpenTick, ulDebugOpenFrameId, ulDebugOpenSubframeId, xDebugOpenRunningHandle);
    xDebugTickOpen = pdFALSE;
}

static void prvDebugPrintInstantTraceEvent(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    if (xTimeline.xDebugEnabled == pdFALSE) {
        return;
    }

    UART_puts("event t=");
    UART_put_u32((uint32_t) xTimeline.xLastTickSeen);
    UART_puts(" frame=");
    UART_put_u32(xTimeline.ulFrameId);
    UART_puts(" sub=");
    UART_put_u32(ulSubframeId);
    UART_puts(" ");

    {
        const TimelineTraceEvent_t xEvent = {
            .xTick = xTimeline.xLastTickSeen,
            .ulFrameId = xTimeline.ulFrameId,
            .ulSubframeId = ulSubframeId,
            .uxTaskIndex = uxTaskIndex,
            .xType = xType
        };

        prvDebugPrintTraceEvent(&xEvent);
    }

    UART_puts("\r\n");
}

static void prvDebugPrintInstantCtxEvent(TaskHandle_t xFrom, TaskHandle_t xTo, uint32_t ulSubframeId)
{
    if (xTimeline.xDebugEnabled == pdFALSE) {
        return;
    }

    UART_puts("event t=");
    UART_put_u32((uint32_t) xTimeline.xLastTickSeen);
    UART_puts(" frame=");
    UART_put_u32(xTimeline.ulFrameId);
    UART_puts(" sub=");
    UART_put_u32(ulSubframeId);
    UART_puts(" ctx ");
    UART_puts(prvTaskNameFromHandle(xFrom));
    UART_puts(" -> ");
    UART_puts(prvTaskNameFromHandle(xTo));
    UART_puts("\r\n");
}

static void prvDebugUpdateRunningTask(TaskHandle_t xSelectedHandle, uint32_t ulCurrentSubframe)
{
    if ((xTimeline.xDebugEnabled == pdFALSE) || (xDebugTickOpen == pdFALSE)) {
        return;
    }

    if (xSelectedHandle != xDebugOpenRunningHandle) {
        if (ulDebugOpenCtxEventCount < TIMELINE_DEBUG_CTX_EVENTS_MAX) {
            xDebugOpenCtxEvents[ulDebugOpenCtxEventCount].xFrom = xDebugOpenRunningHandle;
            xDebugOpenCtxEvents[ulDebugOpenCtxEventCount].xTo = xSelectedHandle;
            ulDebugOpenCtxEventCount++;
        }

        prvDebugPrintInstantCtxEvent(xDebugOpenRunningHandle, xSelectedHandle, ulCurrentSubframe);
        xDebugOpenRunningHandle = xSelectedHandle;
    }
}

static const char * prvTaskNameFromIndex(UBaseType_t uxTaskIndex)
{
    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return "-";
    }

    if (xTimeline.pxConfig->pxTasks[uxTaskIndex].pcName == NULL) {
        return "-";
    }

    return xTimeline.pxConfig->pxTasks[uxTaskIndex].pcName;
}

static const char * prvTaskNameFromHandle(TaskHandle_t xHandle)
{
    uint32_t ulIdx;

    if (xHandle == NULL) {
        return "idle";
    }

    if ((xTimeline.pxConfig == NULL) || (xTimeline.xConfigValid == pdFALSE)) {
        return "unknown";
    }

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if (xTimeline.xRuntime[ulIdx].xHandle == xHandle) {
            return prvTaskNameFromIndex((UBaseType_t) ulIdx);
        }
    }

    return "idle";
}

static uint32_t prvComputeCurrentSubframe(void)
{
    TickType_t xTicksFromFrame;

    if ((xTimeline.pxConfig == NULL) || (xTimeline.xSubframeTicks == 0)) {
        return 0;
    }

    xTicksFromFrame = xTimeline.xLastTickSeen - xTimeline.xFrameStartTick;
    return (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
}

static TaskHandle_t prvSelectTimelineTask(TaskHandle_t xDefaultSelected, uint32_t ulCurrentSubframe, TickType_t xTickInSubframe)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return xDefaultSelected;
    }

    /* Non-preemptive HRT: if one HRT is already running and still inside its
     * window, keep it selected until it completes or misses its deadline. */
    if (xTimeline.xLastSelectedHandle != NULL) {
        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if (pxRt->xHandle != xTimeline.xLastSelectedHandle) {
                continue;
            }

            if ((pxTask->xType == TIMELINE_TASK_HRT) &&
                (pxRt->xIsActive != pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
                (pxTask->ulSubframeId == ulCurrentSubframe) &&
                (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
                (xTickInSubframe < pdMS_TO_TICKS(pxTask->ulEndOffsetMs))) {
                return pxRt->xHandle;
            }

            break;
        }
    }

    /* If no HRT is currently locked-in, select the first active HRT (config order). */
    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == ulCurrentSubframe) &&
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
            (xTickInSubframe < pdMS_TO_TICKS(pxTask->ulEndOffsetMs)) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            return pxRt->xHandle;
        }
    }

    /* Keep current SRT running if still active (SRT can span subframes). */
    if (xTimeline.xLastSelectedHandle != NULL) {
        for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if ((pxRt->xHandle == xTimeline.xLastSelectedHandle) &&
                (pxTask->xType == TIMELINE_TASK_SRT) &&
                (pxRt->xIsActive != pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
                return pxRt->xHandle;
            }
        }
    }

    /* Otherwise select the first active SRT in compile-time order. */
    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_SRT) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            return pxRt->xHandle;
        }
    }

    return xDefaultSelected;
}

static BaseType_t prvHasAnyActiveTaskOfType(TimelineTaskType_t xType)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return pdFALSE;
    }

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == xType) &&
            (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static void prvTracePush(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    uint32_t ulNextHead;

    xTrace.xEvents[xTrace.ulHead].xTick = xTimeline.xLastTickSeen;
    xTrace.xEvents[xTrace.ulHead].ulFrameId = xTimeline.ulFrameId;
    xTrace.xEvents[xTrace.ulHead].ulSubframeId = ulSubframeId;
    xTrace.xEvents[xTrace.ulHead].uxTaskIndex = uxTaskIndex;
    xTrace.xEvents[xTrace.ulHead].xType = xType;

    ulNextHead = (xTrace.ulHead + 1) % TIMELINE_TRACE_BUFFER_LEN;
    xTrace.ulHead = ulNextHead;

    if (xTrace.ulHead == xTrace.ulTail) {
        xTrace.ulTail = (xTrace.ulTail + 1) % TIMELINE_TRACE_BUFFER_LEN;
    }
}

static void prvTracePushFromTask(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    taskENTER_CRITICAL();
    prvTracePush(xType, uxTaskIndex, ulSubframeId);
    taskEXIT_CRITICAL();
    prvDebugPrintInstantTraceEvent(xType, uxTaskIndex, ulSubframeId);
}

static void prvTracePushFromISR(TimelineTraceEventType_t xType, UBaseType_t uxTaskIndex, uint32_t ulSubframeId)
{
    UBaseType_t uxSavedInterruptStatus;

    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    prvTracePush(xType, uxTaskIndex, ulSubframeId);
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
    prvDebugPrintInstantTraceEvent(xType, uxTaskIndex, ulSubframeId);
}

static void prvReleaseTaskFromTaskContext(UBaseType_t uxTaskIndex)
{
    TimelineTaskRuntime_t * pxRt;

    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return;
    }

    pxRt = &xTimeline.xRuntime[uxTaskIndex];
    if ((pxRt->xHandle == NULL) || (pxRt->xDeadlineMissPendingKill != pdFALSE)) {
        return;
    }
    if (pxRt->xIsActive != pdFALSE) {
        return;
    }

    taskENTER_CRITICAL();
    pxRt->xIsActive = pdTRUE;
    pxRt->xCompletedInWindow = pdFALSE;
    pxRt->ulReleaseCount++;
    taskEXIT_CRITICAL();

    vTaskResume(pxRt->xHandle);
    xTaskNotifyGive(pxRt->xHandle);
}

static void prvReleaseTaskFromISR(UBaseType_t uxTaskIndex, BaseType_t * pxHigherPriorityTaskWoken)
{
    TimelineTaskRuntime_t * pxRt;
    BaseType_t xResumedTaskWoken = pdFALSE;

    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return;
    }

    pxRt = &xTimeline.xRuntime[uxTaskIndex];
    if ((pxRt->xHandle == NULL) || (pxRt->xDeadlineMissPendingKill != pdFALSE)) {
        return;
    }
    if (pxRt->xIsActive != pdFALSE) {
        return;
    }

    pxRt->xIsActive = pdTRUE;
    pxRt->xCompletedInWindow = pdFALSE;
    pxRt->ulReleaseCount++;

    xResumedTaskWoken = xTaskResumeFromISR(pxRt->xHandle);
    vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);

    if ((pxHigherPriorityTaskWoken != NULL) && (xResumedTaskWoken != pdFALSE)) {
        *pxHigherPriorityTaskWoken = pdTRUE;
    }
}

static BaseType_t prvTryReleaseNextSrtFromTaskContext(uint32_t ulCurrentSubframe)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return pdFALSE;
    }

    if (prvHasAnyActiveTaskOfType(TIMELINE_TASK_HRT) != pdFALSE) {
        return pdFALSE;
    }

    if (prvHasAnyActiveTaskOfType(TIMELINE_TASK_SRT) != pdFALSE) {
        return pdFALSE;
    }

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_SRT) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive == pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            prvReleaseTaskFromTaskContext((UBaseType_t) ulIdx);
            prvTracePushFromTask(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, ulCurrentSubframe);
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static BaseType_t prvTryReleaseNextSrtFromISR(uint32_t ulCurrentSubframe, BaseType_t * pxHigherPriorityTaskWoken)
{
    uint32_t ulIdx;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return pdFALSE;
    }

    if (prvHasAnyActiveTaskOfType(TIMELINE_TASK_HRT) != pdFALSE) {
        return pdFALSE;
    }

    if (prvHasAnyActiveTaskOfType(TIMELINE_TASK_SRT) != pdFALSE) {
        return pdFALSE;
    }

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_SRT) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xIsActive == pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            prvReleaseTaskFromISR((UBaseType_t) ulIdx, pxHigherPriorityTaskWoken);
            prvTracePushFromISR(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, ulCurrentSubframe);
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static void prvTimelineManagedTask(void * pvArg)
{
    TimelineTaskContext_t * pxCtx = (TimelineTaskContext_t *) pvArg;
    const UBaseType_t uxIndex = pxCtx->uxIndex;

    for (;;) {
        const TimelineTaskConfig_t * pxTaskCfg;

        (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if ((xTimeline.pxConfig == NULL) || (uxIndex >= xTimeline.pxConfig->ulTaskCount)) {
            continue;
        }

        pxTaskCfg = &xTimeline.pxConfig->pxTasks[uxIndex];
        pxCtx->xExecInfo.uxTaskIndex = uxIndex;
        pxCtx->xExecInfo.ulSubframeId = pxTaskCfg->ulSubframeId;
        pxCtx->xExecInfo.ulStartOffsetMs = pxTaskCfg->ulStartOffsetMs;
        pxCtx->xExecInfo.ulEndOffsetMs = pxTaskCfg->ulEndOffsetMs;
        pxCtx->xExecInfo.ulRunDurationMs = (pxTaskCfg->ulEndOffsetMs > pxTaskCfg->ulStartOffsetMs) ?
                                           (pxTaskCfg->ulEndOffsetMs - pxTaskCfg->ulStartOffsetMs) :
                                           0U;

        pxTaskCfg->pxTaskCode((void *) &pxCtx->xExecInfo);
        vTimelineSchedulerTaskCompletedFromTaskContext(uxIndex);

        /* Keep timeline tasks out of the generic ready lists when not released.
         * Next activation happens through explicit resume + notify. */
        vTaskSuspend(NULL);
    }
}

static BaseType_t prvValidateConfig(const TimelineConfig_t * pxConfig)
{
    uint32_t ulIdx;
    TickType_t xMajorFrameTicks;
    TickType_t xSubframeTicks;
    uint32_t ulSubframeCount;

    if ((pxConfig == NULL) || (pxConfig->pxTasks == NULL) || (pxConfig->ulTaskCount == 0)) {
        return pdFALSE;
    }

    if ((pxConfig->ulTaskCount > TIMELINE_MAX_TASKS) || (pxConfig->ulSubframeMs == 0) ||
        (pxConfig->ulMajorFrameMs == 0) || ((pxConfig->ulMajorFrameMs % pxConfig->ulSubframeMs) != 0)) {
        return pdFALSE;
    }

    xMajorFrameTicks = pdMS_TO_TICKS(pxConfig->ulMajorFrameMs);
    xSubframeTicks = pdMS_TO_TICKS(pxConfig->ulSubframeMs);
    if ((xMajorFrameTicks == 0) || (xSubframeTicks == 0) ||
        ((xMajorFrameTicks % xSubframeTicks) != 0)) {
        return pdFALSE;
    }

    ulSubframeCount = (uint32_t) (xMajorFrameTicks / xSubframeTicks);
    for (ulIdx = 0; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &pxConfig->pxTasks[ulIdx];
        if ((pxTask->pcName == NULL) || (pxTask->pxTaskCode == NULL)) {
            return pdFALSE;
        }

        if (pxTask->ulSubframeId >= ulSubframeCount) {
            return pdFALSE;
        }

        if (pxTask->xType == TIMELINE_TASK_HRT) {
            const TickType_t xStartTick = pdMS_TO_TICKS(pxTask->ulStartOffsetMs);
            const TickType_t xEndTick = pdMS_TO_TICKS(pxTask->ulEndOffsetMs);

            if ((xEndTick <= xStartTick) || (xEndTick > xSubframeTicks)) {
                return pdFALSE;
            }
        }
    }

    /* SRT tasks must remain compact and ordered after all HRT entries. */
    {
        BaseType_t xFoundFirstSrt = pdFALSE;
        BaseType_t xFoundHrtAfterSrt = pdFALSE;

        for (ulIdx = 0; ulIdx < pxConfig->ulTaskCount; ulIdx++) {
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

    configASSERT(xTimeline.pxConfig != NULL);
    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
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
    xTimeline.xTaskContexts[uxIndex].xExecInfo.uxTaskIndex = uxIndex;
    xTimeline.xTaskContexts[uxIndex].xExecInfo.ulSubframeId = 0U;
    xTimeline.xTaskContexts[uxIndex].xExecInfo.ulStartOffsetMs = 0U;
    xTimeline.xTaskContexts[uxIndex].xExecInfo.ulEndOffsetMs = 0U;
    xTimeline.xTaskContexts[uxIndex].xExecInfo.ulRunDurationMs = 0U;
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

    /* Keep managed tasks suspended until timeline release logic explicitly
     * resumes + notifies them. This avoids early executions before release. */
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

void vTimelineSchedulerSetDebugEnabled(BaseType_t xEnable)
{
    xTimeline.xDebugEnabled = (xEnable != pdFALSE) ? pdTRUE : pdFALSE;
}

uint32_t ulTimelineSchedulerTraceRead(TimelineTraceEvent_t * pxBuffer, uint32_t ulMaxEvents)
{
    uint32_t ulReadCount = 0;

    if ((pxBuffer == NULL) || (ulMaxEvents == 0)) {
        return 0;
    }

    taskENTER_CRITICAL();
    while ((xTrace.ulTail != xTrace.ulHead) && (ulReadCount < ulMaxEvents)) {
        pxBuffer[ulReadCount] = xTrace.xEvents[xTrace.ulTail];
        xTrace.ulTail = (xTrace.ulTail + 1) % TIMELINE_TRACE_BUFFER_LEN;
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

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
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

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
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

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
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
    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == 0) &&
            (pdMS_TO_TICKS(pxTask->ulStartOffsetMs) == 0) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            prvReleaseTaskFromTaskContext((UBaseType_t) ulIdx);
            prvTracePushFromTask(TIMELINE_TRACE_EVT_HRT_RELEASE, (UBaseType_t) ulIdx, 0);
            xAnyHrtActive = pdTRUE;
        }
    }

    /* If no HRT is active at frame start, release first SRT immediately. */
    if (xAnyHrtActive == pdFALSE) {
        for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
            TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if ((pxTask->xType == TIMELINE_TASK_SRT) &&
                (pxRt->xHandle != NULL) &&
                (pxRt->xIsActive == pdFALSE) &&
                (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
                (pxRt->xCompletedInWindow == pdFALSE)) {
                prvReleaseTaskFromTaskContext((UBaseType_t) ulIdx);
                prvTracePushFromTask(TIMELINE_TRACE_EVT_SRT_RELEASE, (UBaseType_t) ulIdx, 0);
                break;
            }
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
    xTimeline.xDebugEnabled = pdFALSE;
    xTimeline.xConfigValid = pdTRUE;
    xTimeline.xFrameResetPending = pdFALSE;
    xTimeline.xMaintenanceRequestPending = pdFALSE;
    xTimeline.xLastTickSeen = 0;
    xTimeline.xLastSelectedHandle = NULL;
    xTimeline.ulFrameId = 0;
    xTrace.ulHead = 0;
    xTrace.ulTail = 0;
    xDebugTickOpen = pdFALSE;
    xDebugOpenTick = 0;
    ulDebugOpenFrameId = 0U;
    ulDebugOpenSubframeId = 0U;
    xDebugOpenRunningHandle = NULL;
    ulDebugOpenCtxEventCount = 0U;

    for (ulIdx = 0; ulIdx < TIMELINE_MAX_TASKS; ulIdx++) {
        xTimeline.xRuntime[ulIdx].xHandle = NULL;
        xTimeline.xRuntime[ulIdx].ulReleaseCount = 0;
        xTimeline.xRuntime[ulIdx].ulCompletionCount = 0;
        xTimeline.xRuntime[ulIdx].ulDeadlineMissCount = 0;
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

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        if (prvCreateManagedTaskIfMissing((UBaseType_t) ulIdx) != pdPASS) {
            return pdFAIL;
        }
    }

    return pdPASS;
}

void vTimelineSchedulerKernelStart(TickType_t xStartTick)
{
    configASSERT(xTimeline.xConfigValid == pdTRUE);
    xTimeline.xFrameStartTick = xStartTick;
    xTimeline.xLastTickSeen = xStartTick;
    xTimeline.xStarted = pdTRUE;
    xTimeline.xMaintenanceRequestPending = pdFALSE;
    xTimeline.xLastSelectedHandle = NULL;
    xTimeline.ulFrameId = 0;
    xDebugTickOpen = pdFALSE;
    xDebugOpenRunningHandle = NULL;
    ulDebugOpenCtxEventCount = 0U;
    prvResetFrameRuntimeState();
    prvTracePushFromTask(TIMELINE_TRACE_EVT_FRAME_START, 0, 0);
    prvReleaseFrameStartTasksFromTaskContext();
    prvDebugOpenTick(xStartTick, 0U, 0U);
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

    prvDebugFinalizeOpenTickIfNeeded(xNowTick);

    xTimeline.xLastTickSeen = xNowTick;
    xTicksFromFrame = xNowTick - xTimeline.xFrameStartTick;
    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);

    if (xTicksFromFrame >= xTimeline.xMajorFrameTicks) {
        prvMarkFrameBoundaryCarryOverFromISR(ulCurrentSubframe);
        xTimeline.xFrameStartTick = xNowTick;
        xTimeline.xFrameResetPending = pdTRUE;
        xTicksFromFrame = 0;
        xTimeline.ulFrameId++;
        prvResetFrameRuntimeState();
        prvTracePushFromISR(TIMELINE_TRACE_EVT_FRAME_START, 0, 0);
    }

    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
    xTickInSubframe = xTicksFromFrame % xTimeline.xSubframeTicks;

    for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        const TimelineTaskConfig_t * pxTask = &xTimeline.pxConfig->pxTasks[ulIdx];
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if ((pxTask->xType == TIMELINE_TASK_HRT) &&
            (pxTask->ulSubframeId == ulCurrentSubframe) &&
            (xTickInSubframe == pdMS_TO_TICKS(pxTask->ulStartOffsetMs)) &&
            (pxRt->xHandle != NULL) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            prvReleaseTaskFromISR((UBaseType_t) ulIdx, pxHigherPriorityTaskWoken);
            prvTracePushFromISR(TIMELINE_TRACE_EVT_HRT_RELEASE, (UBaseType_t) ulIdx, ulCurrentSubframe);
        }

        if ((pxTask->xType == TIMELINE_TASK_HRT) && (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
            ((ulCurrentSubframe != pxTask->ulSubframeId) ||
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulEndOffsetMs)))) {
            pxRt->xDeadlineMissPendingKill = pdTRUE;
            pxRt->ulDeadlineMissCount++;
            pxRt->xIsActive = pdFALSE;
            prvTracePushFromISR(TIMELINE_TRACE_EVT_DEADLINE_MISS, (UBaseType_t) ulIdx, ulCurrentSubframe);
        }
    }

    (void) prvTryReleaseNextSrtFromISR(ulCurrentSubframe, pxHigherPriorityTaskWoken);

    if ((xTimeline.xMaintenanceRequestPending == pdFALSE) &&
        (prvAnyPendingMaintenanceFromISR() != pdFALSE)) {
        BaseType_t xTimerTaskWoken = pdFALSE;

        if (xTimerPendFunctionCallFromISR(prvTimelineMaintenanceCallback, NULL, 0, &xTimerTaskWoken) == pdPASS) {
            xTimeline.xMaintenanceRequestPending = pdTRUE;

            if ((pxHigherPriorityTaskWoken != NULL) && (xTimerTaskWoken != pdFALSE)) {
                *pxHigherPriorityTaskWoken = pdTRUE;
            }
        }
    }

    if ((xDebugTickOpen == pdFALSE) || (xDebugOpenTick != xNowTick)) {
        prvDebugOpenTick(xNowTick, xTimeline.ulFrameId, ulCurrentSubframe);
    }
}

TaskHandle_t xTimelineSchedulerSelectNextTask(TaskHandle_t xDefaultSelected, TickType_t xNowTick)
{
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;
    TaskHandle_t xSelectedHandle;

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
    xSelectedHandle = prvSelectTimelineTask(xDefaultSelected, ulCurrentSubframe, xTickInSubframe);
    xTimeline.xLastSelectedHandle = xSelectedHandle;
    prvDebugUpdateRunningTask(xSelectedHandle, ulCurrentSubframe);
    return xSelectedHandle;
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

    {
        const uint32_t ulCurrentSubframe = prvComputeCurrentSubframe();

        prvTracePushFromTask(TIMELINE_TRACE_EVT_TASK_COMPLETE, uxTaskIndex, ulCurrentSubframe);
        (void) prvTryReleaseNextSrtFromTaskContext(ulCurrentSubframe);
    }
}

const TimelineTaskRuntime_t * pxTimelineSchedulerGetRuntime(uint32_t * pulTaskCount)
{
    if ((pulTaskCount != NULL) && (xTimeline.pxConfig != NULL)) {
        *pulTaskCount = xTimeline.pxConfig->ulTaskCount;
    }

    return &xTimeline.xRuntime[0];
}
