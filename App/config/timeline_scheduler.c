#include "timeline_scheduler.h"
#include "timers.h"
#include "uart.h"
#include <string.h>

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
    TaskHandle_t xLastSelectedHandle;
    TaskHandle_t xAccountingHandle;
    TickType_t xLastDispatchTick[TIMELINE_MAX_TASKS];
    uint32_t ulFrameId;
    TimelineTaskContext_t xTaskContexts[TIMELINE_MAX_TASKS];
    TimelineTaskRuntime_t xRuntime[TIMELINE_MAX_TASKS];
} TimelineSchedulerState_t;

static TimelineSchedulerState_t xTimeline;

#if ( DEBUG == 1 )
#define TIMELINE_DEBUG_IDLE_NAME    "IDLE"
#define TIMELINE_DEBUG_TIMER_NAME   "Tmr Svc"

static const char * prvDebugNormalizeTaskName(const char * pcName)
{
    const size_t uxIdleNameLen = sizeof(TIMELINE_DEBUG_IDLE_NAME) - 1U;
    const size_t uxTimerNameLen = sizeof(TIMELINE_DEBUG_TIMER_NAME) - 1U;

    if (pcName == NULL) {
        return TIMELINE_DEBUG_IDLE_NAME;
    }

    if (strncmp(pcName, TIMELINE_DEBUG_IDLE_NAME, uxIdleNameLen) == 0) {
        return TIMELINE_DEBUG_IDLE_NAME;
    }

    if (strncmp(pcName, TIMELINE_DEBUG_TIMER_NAME, uxTimerNameLen) == 0) {
        return TIMELINE_DEBUG_IDLE_NAME;
    }

    return pcName;
}

static const char * prvDebugTaskNameOrIdle(TaskHandle_t xHandle)
{
    return prvDebugNormalizeTaskName((xHandle != NULL) ? pcTaskGetName(xHandle) : NULL);
}

static void prvDebugPrintContextSwitch(const char * pcFromName, const char * pcToName)
{
    UART_puts("\tevent: context switch ");
    UART_puts(prvDebugNormalizeTaskName(pcFromName));
    UART_puts(" -> ");
    UART_puts(prvDebugNormalizeTaskName(pcToName));
    UART_puts("\r\n");
    UART_puts("\trunning: [");
    UART_puts(prvDebugNormalizeTaskName(pcToName));
    UART_puts("]\r\n");
}

static void prvDebugPrintMajorFrameStart(uint32_t ulFrameId)
{
    UART_puts("Start major frame ");
    UART_put_u32(ulFrameId);
    UART_puts("\r\n");
}
#endif

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
            (pxRt->xIsActive == pdTRUE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            return pdTRUE;
        }
    }

    return pdFALSE;
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
    pxRt->xExecutedTicksInActivation = 0U;
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
    pxRt->xExecutedTicksInActivation = 0U;
    pxRt->ulReleaseCount++;

    xResumedTaskWoken = xTaskResumeFromISR(pxRt->xHandle);
    vTaskNotifyGiveFromISR(pxRt->xHandle, pxHigherPriorityTaskWoken);

    if ((pxHigherPriorityTaskWoken != NULL) && (xResumedTaskWoken != pdFALSE)) {
        *pxHigherPriorityTaskWoken = pdTRUE;
    }
}

static BaseType_t prvTryReleaseNextSrtFromTaskContext(void)
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
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static BaseType_t prvTryReleaseNextSrtFromISR(uint32_t ulCurrentSubframe, BaseType_t * pxHigherPriorityTaskWoken)
{
    uint32_t ulIdx;
    (void) ulCurrentSubframe;

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
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static void prvAccountRunningTaskExecutionTickFromISR(void)
{
    uint32_t ulIdx;
    TaskHandle_t xRunningHandle = xTimeline.xAccountingHandle;

    if ((xTimeline.xConfigValid == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    if (xRunningHandle == NULL) {
        return;
    }

    for (ulIdx = 0U; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
        TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

        if (pxRt->xHandle != xRunningHandle) {
            continue;
        }

        if ((pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE)) {
            pxRt->xExecutedTicksInActivation++;
        }
        break;
    }
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
        pxTaskCfg->pxTaskCode((void *) &pxCtx->uxIndex);

        if (pxTaskCfg->xType == TIMELINE_TASK_SRT) {
            /* Keep SRT completion aligned to a tick boundary so SRT hand-over
             * is visible on the next scheduler tick. */
            while (xTaskGetTickCount() <= xTimeline.xLastDispatchTick[uxIndex]) {
                taskYIELD();
            }
        }

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
        xTimeline.xRuntime[ulIdx].xExecutedTicksInActivation = 0U;
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
                    &xHandle) == pdFAIL) {
        return pdFAIL;
    }

    xTimeline.xRuntime[uxIndex].xHandle = xHandle;
    xTimeline.xRuntime[uxIndex].xIsActive = pdFALSE;
    xTimeline.xRuntime[uxIndex].xCompletedInWindow = pdFALSE;
    xTimeline.xRuntime[uxIndex].xDeadlineMissPendingKill = pdFALSE;
    xTimeline.xRuntime[uxIndex].xExecutedTicksInActivation = 0U;

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
            xTimeline.xRuntime[ulIdx].xExecutedTicksInActivation = 0U;
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
    (void) ulCurrentSubframe;

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

#if ( DEBUG == 1 )
        UART_puts("\t[");
        UART_puts(pxTask->pcName);
        UART_puts("] terminated -> deadline miss\r\n");
#endif

        if (pxTask->xType == TIMELINE_TASK_HRT) {
            pxRt->ulDeadlineMissCount++;
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
    xTimeline.xConfigValid = pdTRUE;
    xTimeline.xFrameResetPending = pdFALSE;
    xTimeline.xMaintenanceRequestPending = pdFALSE;
    xTimeline.xLastTickSeen = 0;
    xTimeline.xLastSelectedHandle = NULL;
    xTimeline.xAccountingHandle = NULL;
    xTimeline.ulFrameId = 0;

    for (ulIdx = 0; ulIdx < TIMELINE_MAX_TASKS; ulIdx++) {
        xTimeline.xLastDispatchTick[ulIdx] = 0;
        xTimeline.xRuntime[ulIdx].xHandle = NULL;
        xTimeline.xRuntime[ulIdx].ulReleaseCount = 0;
        xTimeline.xRuntime[ulIdx].ulCompletionCount = 0;
        xTimeline.xRuntime[ulIdx].ulDeadlineMissCount = 0;
        xTimeline.xRuntime[ulIdx].xExecutedTicksInActivation = 0;
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
        if (prvCreateManagedTaskIfMissing((UBaseType_t) ulIdx) == pdFAIL) {
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
    xTimeline.xAccountingHandle = NULL;
    xTimeline.ulFrameId = 0;
    prvResetFrameRuntimeState();
    prvReleaseFrameStartTasksFromTaskContext();
    xTimeline.xAccountingHandle = prvSelectTimelineTask(NULL, 0U, 0U);

#if ( DEBUG == 1 )
    {
        TaskHandle_t xBootSelected = prvSelectTimelineTask(NULL, 0U, 0U);

        prvDebugPrintMajorFrameStart(xTimeline.ulFrameId);
        if (xBootSelected != NULL) {
            prvDebugPrintContextSwitch(TIMELINE_DEBUG_IDLE_NAME, pcTaskGetName(xBootSelected));
        }
    }
#endif
}

void vTimelineSchedulerOnTickFromISR(TickType_t xNowTick, BaseType_t * pxHigherPriorityTaskWoken)
{
    uint32_t ulIdx;
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;
    const char * pcDeadlineMissTaskName = NULL;
    TaskHandle_t xPostTickSelected;
#if ( DEBUG == 1 )
    TaskHandle_t xPrevSelectedHandle;
#endif

    if ((xTimeline.xStarted == pdFALSE) || (xTimeline.pxConfig == NULL)) {
        return;
    }

    /* Account one execution tick for the task that ran in
     * [xNowTick - 1, xNowTick). */
    prvAccountRunningTaskExecutionTickFromISR();

#if ( DEBUG == 1 )
    xPrevSelectedHandle = xTimeline.xLastSelectedHandle;
#endif
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
#if ( DEBUG == 1 )
        prvDebugPrintMajorFrameStart(xTimeline.ulFrameId);
#endif
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
        }

        if ((pxTask->xType == TIMELINE_TASK_HRT) && (pxRt->xIsActive != pdFALSE) &&
            (pxRt->xCompletedInWindow == pdFALSE) &&
            (pxRt->xDeadlineMissPendingKill == pdFALSE) &&
            ((ulCurrentSubframe != pxTask->ulSubframeId) ||
            (xTickInSubframe >= pdMS_TO_TICKS(pxTask->ulEndOffsetMs)))) {
            pxRt->xDeadlineMissPendingKill = pdTRUE;
            pxRt->ulDeadlineMissCount++;
            pxRt->xIsActive = pdFALSE;
            if (pcDeadlineMissTaskName == NULL) {
                pcDeadlineMissTaskName = pxTask->pcName;
            }
        }
    }

    (void) prvTryReleaseNextSrtFromISR(ulCurrentSubframe, pxHigherPriorityTaskWoken);
    xPostTickSelected = prvSelectTimelineTask(NULL, ulCurrentSubframe, xTickInSubframe);
    xTimeline.xAccountingHandle = xPostTickSelected;

#if ( DEBUG == 1 )
    if (pcDeadlineMissTaskName != NULL) {
        const char * pcPostSwitchName = prvDebugTaskNameOrIdle(xPostTickSelected);

        UART_puts("\t[");
        UART_puts(pcDeadlineMissTaskName);
        UART_puts("] terminated -> deadline miss\r\n");
        if (strcmp(prvDebugNormalizeTaskName(pcDeadlineMissTaskName), pcPostSwitchName) != 0) {
            prvDebugPrintContextSwitch(pcDeadlineMissTaskName, pcPostSwitchName);
        }
    } else {
        const char * pcPrevSwitchName = prvDebugTaskNameOrIdle(xPrevSelectedHandle);
        const char * pcPostSwitchName = prvDebugTaskNameOrIdle(xPostTickSelected);

        if (strcmp(pcPrevSwitchName, pcPostSwitchName) != 0) {
            prvDebugPrintContextSwitch(pcPrevSwitchName, pcPostSwitchName);
        }
    }
#endif

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
}

TaskHandle_t xTimelineSchedulerSelectNextTask(TaskHandle_t xDefaultSelected, TickType_t xNowTick)
{
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;
    uint32_t ulIdx;
    TaskHandle_t xPrevSelectedHandle;
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
    xPrevSelectedHandle = xTimeline.xLastSelectedHandle;
    xSelectedHandle = prvSelectTimelineTask(xDefaultSelected, ulCurrentSubframe, xTickInSubframe);

    if (xSelectedHandle != xPrevSelectedHandle) {
        for (ulIdx = 0; ulIdx < xTimeline.pxConfig->ulTaskCount; ulIdx++) {
            const TimelineTaskRuntime_t * pxRt = &xTimeline.xRuntime[ulIdx];

            if (pxRt->xHandle == xSelectedHandle) {
                xTimeline.xLastDispatchTick[ulIdx] = xTimeline.xLastTickSeen;
                break;
            }
        }
    }

    xTimeline.xLastSelectedHandle = xSelectedHandle;
    return xSelectedHandle;
}

void vTimelineSchedulerTaskCompletedFromTaskContext(UBaseType_t uxTaskIndex)
{
    BaseType_t xCanCountCompletion = pdFALSE;
    TaskHandle_t xPostCompletionSelected;
    TickType_t xTicksFromFrame;
    TickType_t xTickInSubframe;
    uint32_t ulCurrentSubframe;
#if ( DEBUG == 1 )
    TaskHandle_t xPrevSelectedHandle;
#endif

    if ((xTimeline.pxConfig == NULL) || (uxTaskIndex >= xTimeline.pxConfig->ulTaskCount)) {
        return;
    }

#if ( DEBUG == 1 )
    xPrevSelectedHandle = xTimeline.xLastSelectedHandle;
#endif

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

#if ( DEBUG == 1 )
    UART_puts("\t[");
    UART_puts(xTimeline.pxConfig->pxTasks[uxTaskIndex].pcName);
    UART_puts("] completed\r\n");
#endif

    /* Release the next SRT when no active HRT/SRT is present. */
    (void) prvTryReleaseNextSrtFromTaskContext();

    xTicksFromFrame = xTimeline.xLastTickSeen - xTimeline.xFrameStartTick;
    ulCurrentSubframe = (uint32_t) (xTicksFromFrame / xTimeline.xSubframeTicks);
    xTickInSubframe = xTicksFromFrame % xTimeline.xSubframeTicks;
    xPostCompletionSelected = prvSelectTimelineTask(NULL, ulCurrentSubframe, xTickInSubframe);
    xTimeline.xAccountingHandle = xPostCompletionSelected;

#if ( DEBUG == 1 )
    {
        const char * pcPrevSwitchName = prvDebugTaskNameOrIdle(xPrevSelectedHandle);
        const char * pcPostSwitchName = prvDebugTaskNameOrIdle(xPostCompletionSelected);

        if (strcmp(pcPrevSwitchName, pcPostSwitchName) != 0) {
            prvDebugPrintContextSwitch(pcPrevSwitchName, pcPostSwitchName);
        }
    }
#endif
}

TickType_t xTimelineSchedulerGetTaskExecutedTicks(UBaseType_t uxTaskIndex)
{
    TickType_t xExecutedTicks = 0U;

    taskENTER_CRITICAL();
    if ((xTimeline.pxConfig != NULL) && (uxTaskIndex < xTimeline.pxConfig->ulTaskCount)) {
        xExecutedTicks = xTimeline.xRuntime[uxTaskIndex].xExecutedTicksInActivation;
    }
    taskEXIT_CRITICAL();

    return xExecutedTicks;
}

const TimelineTaskRuntime_t * pxTimelineSchedulerGetRuntime(uint32_t * pulTaskCount)
{
    if ((pulTaskCount != NULL) && (xTimeline.pxConfig != NULL)) {
        *pulTaskCount = xTimeline.pxConfig->ulTaskCount;
    }

    return &xTimeline.xRuntime[0];
}
