#ifndef TIMELINE_CONFIG_H
#define TIMELINE_CONFIG_H

#include "FreeRTOS.h"
#include "task.h"

typedef enum {
    TASK_TYPE_HRT, 
    TASK_TYPE_SRT  
} TimelineTaskType_t;

typedef struct {
    const char* task_name;
    TaskFunction_t function;
    TimelineTaskType_t type;
    uint32_t ulStart_time_ms;   // Offset dall'inizio del Major Frame
    uint32_t ulEnd_time_ms;     // Deadline assoluta nel Major Frame
    uint32_t ulSubframe_id;     // ID del sotto-frame (opzionale, per raggruppamento)
} TimelineTaskConfig_t;

typedef struct {
    uint32_t major_frame_ms;    // Durata totale (es. 1000 ms)
    uint32_t subframe_count;    // Numero di sotto-frame
    uint32_t task_count;        // Numero totale di task
    TimelineTaskConfig_t *tasks;// Array dei task
} TimelineConfig_t;

const TimelineConfig_t* get_system_configuration(void);

#endif // TIMELINE_CONFIG_H
