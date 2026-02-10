// Timeline-driven scheduler implementation.
// This module owns the scheduler task and enforces the major/sub-frame timeline.

#include "timeline_scheduler.h"
#include <stdbool.h>

typedef struct {
    TaskFunction_t function;
    TaskHandle_t scheduler_handle;
} TaskWrapperCtx_t;

typedef struct {
    const TimelineConfig_t *cfg;
} SchedulerState_t;

static SchedulerState_t g_scheduler;

// Wrapper used to notify the scheduler when a task completes.
static void TimelineTaskWrapper(void *arg)
{
    TaskWrapperCtx_t *ctx = (TaskWrapperCtx_t *)arg;
    ctx->function(NULL);
    (void)xTaskNotifyGive(ctx->scheduler_handle);
    vTaskDelete(NULL);
}

// Delay until an absolute tick value (best-effort; no tickless idle handling).
static void DelayUntilTick(TickType_t target_tick)
{
    TickType_t now = xTaskGetTickCount();
    if (now < target_tick) {
        vTaskDelay(target_tick - now);
    }
}

// Spawn a task at start_tick and wait until deadline_tick. If the task exceeds
// the deadline, it is deleted.
static bool RunTaskWithDeadline(const TimelineTaskConfig_t *tc,
                                TickType_t start_tick,
                                TickType_t deadline_tick)
{
    DelayUntilTick(start_tick);

    TaskWrapperCtx_t ctx = { tc->function, xTaskGetCurrentTaskHandle() };
    TaskHandle_t task_handle = NULL;
    if (xTaskCreate(TimelineTaskWrapper,
                    tc->task_name,
                    tc->stack_words,
                    &ctx,
                    tc->priority,
                    &task_handle) != pdPASS) {
        return false;
    }

    TickType_t now = xTaskGetTickCount();
    if (now < deadline_tick) {
        TickType_t timeout = deadline_tick - now;
        if (ulTaskNotifyTake(pdTRUE, timeout) > 0) {
            return true;
        }
    }

    // Deadline exceeded: terminate the task.
    vTaskDelete(task_handle);
    return false;
}

// Run the tasks scheduled inside a sub-frame.
static void RunSubframe(uint32_t subframe_id, TickType_t subframe_start,
                        TickType_t subframe_end)
{
    const TimelineConfig_t *cfg = g_scheduler.cfg;

    // 1) HRT tasks, in list order, for this sub-frame.
    // Assumption: HRT tasks are listed in non-decreasing start_time_ms order
    // within the same sub-frame.
    for (uint32_t i = 0; i < cfg->task_count; i++) {
        const TimelineTaskConfig_t *tc = &cfg->tasks[i];
        if (tc->type != TASK_HARD_RT || tc->subframe_id != subframe_id) {
            continue;
        }

        TickType_t start = subframe_start + pdMS_TO_TICKS(tc->start_time_ms);
        TickType_t end = subframe_start + pdMS_TO_TICKS(tc->end_time_ms);
        if (end > subframe_end) {
            end = subframe_end;
        }

        (void)RunTaskWithDeadline(tc, start, end);
    }

    // 2) SRT tasks during remaining slack, in fixed list order.
    for (uint32_t i = 0; i < cfg->task_count; i++) {
        const TimelineTaskConfig_t *tc = &cfg->tasks[i];
        if (tc->type != TASK_SOFT_RT) {
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        if (now >= subframe_end) {
            break;
        }

        (void)RunTaskWithDeadline(tc, now, subframe_end);
    }
}

// Scheduler main loop: iterates sub-frames inside a major frame.
static void TimelineSchedulerTask(void *arg)
{
    (void)arg;
    const TimelineConfig_t *cfg = g_scheduler.cfg;
    const TickType_t subframe_ticks = pdMS_TO_TICKS(cfg->subframe_ms);
    const uint32_t subframe_count = cfg->major_frame_ms / cfg->subframe_ms;

    TickType_t frame_start = xTaskGetTickCount();
    for (;;) {
        for (uint32_t sf = 0; sf < subframe_count; sf++) {
            TickType_t subframe_start = frame_start + (sf * subframe_ticks);
            TickType_t subframe_end = subframe_start + subframe_ticks;

            RunSubframe(sf, subframe_start, subframe_end);

            // Align to sub-frame boundary if tasks finish early.
            DelayUntilTick(subframe_end);
        }

        // Major frame boundary: reset timeline.
        TickType_t next_frame_start = frame_start + pdMS_TO_TICKS(cfg->major_frame_ms);
        DelayUntilTick(next_frame_start);
        frame_start = next_frame_start;
    }
}

void vConfigureScheduler(const TimelineConfig_t *cfg)
{
    g_scheduler.cfg = cfg;
}

BaseType_t xStartTimelineScheduler(UBaseType_t priority, uint16_t stack_words)
{
    // Scheduler runs at a higher priority than HRT tasks,
    // but blocks during task execution so HRT can run.
    return xTaskCreate(TimelineSchedulerTask,
                       "TimelineScheduler",
                       stack_words,
                       NULL,
                       priority,
                       NULL);
}
