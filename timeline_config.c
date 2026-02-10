// Example tasks and timeline configuration.
// This keeps application-specific tasks separate from the scheduler core.

#include "timeline_config.h"
#include "task.h"

// ---- Example tasks -------------------------------------------------------
// Replace these with your real application tasks.

static void Task_A(void *arg) { (void)arg; /* do work */ }
static void Task_B(void *arg) { (void)arg; /* do work */ }
static void Task_X(void *arg) { (void)arg; /* do best-effort work */ }
static void Task_Y(void *arg) { (void)arg; /* do best-effort work */ }

// ---- Example configuration -----------------------------------------------
// HRT tasks should be listed in non-decreasing start_time_ms order within
// the same sub-frame. SRT tasks are executed in the order listed here.

static const TimelineTaskConfig_t g_tasks[] = {
    { "Task_A", Task_A, TASK_HARD_RT, 1, 7, 2, tskIDLE_PRIORITY + 3, 256 },
    { "Task_B", Task_B, TASK_HARD_RT, 0, 4, 5, tskIDLE_PRIORITY + 3, 256 },
    { "Task_X", Task_X, TASK_SOFT_RT, 0, 0, 0, tskIDLE_PRIORITY + 1, 256 },
    { "Task_Y", Task_Y, TASK_SOFT_RT, 0, 0, 0, tskIDLE_PRIORITY + 1, 256 },
};

const TimelineConfig_t g_timeline = {
    .major_frame_ms = 100,
    .subframe_ms = 10,
    .tasks = g_tasks,
    .task_count = sizeof(g_tasks) / sizeof(g_tasks[0]),
};
