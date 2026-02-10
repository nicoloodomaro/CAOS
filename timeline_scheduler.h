// Timeline-driven scheduler public interface.
// This header defines the configuration model and the APIs used by main.c.

#ifndef TIMELINE_SCHEDULER_H
#define TIMELINE_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

// Task category used by the timeline scheduler.
typedef enum {
    TASK_HARD_RT = 0,
    TASK_SOFT_RT = 1
} TaskType_t;

// Per-task configuration entry.
// Notes:
// - For HRT tasks, start_time_ms/end_time_ms are relative to the sub-frame.
// - For SRT tasks, start_time_ms/end_time_ms are ignored.
typedef struct {
    const char *task_name;
    TaskFunction_t function;
    TaskType_t type;
    uint32_t start_time_ms;   // Offset within sub-frame (HRT only)
    uint32_t end_time_ms;     // Deadline within sub-frame (HRT only)
    uint32_t subframe_id;     // Which sub-frame the task belongs to (HRT only)
    UBaseType_t priority;     // Priority to use when spawned
    uint16_t stack_words;     // Stack size in words
} TimelineTaskConfig_t;

// Global timeline configuration for the scheduler.
typedef struct {
    uint32_t major_frame_ms;
    uint32_t subframe_ms;
    const TimelineTaskConfig_t *tasks;
    uint32_t task_count;
} TimelineConfig_t;

// Load configuration. Call once before starting the scheduler task.
void vConfigureScheduler(const TimelineConfig_t *cfg);

// Create the scheduler task. Returns pdPASS/pdFAIL.
BaseType_t xStartTimelineScheduler(UBaseType_t priority, uint16_t stack_words);

#endif // TIMELINE_SCHEDULER_H
