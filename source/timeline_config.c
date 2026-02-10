#include "timeline_config.h"
#include "utils.h"

// --- TASK GENERICI ---
// Usiamo funzioni generiche per non dover scrivere 5 funzioni uguali.

// Task HRT: Simula un carico di lavoro
void Task_HRT_Generic(void *p) {
    // Non stampa nulla, lavora e basta.
    // Lo scheduler segnalerà la sua presenza.
    while(1) {
        // Busy wait per occupare CPU
        for(volatile int i=0; i<1000; i++); 
    }
}

// Task SRT: Simula background
void Task_SRT_Generic(void *p) {
    while(1) {
        // Lavoro a bassa priorità
        for(volatile int i=0; i<1000; i++);
    }
}

static TimelineTaskConfig_t my_tasks[] = {
    // --- IL PIANO DI VOLO (Major Frame: 20 Tick) ---

    // 1. [HRT] Sensori: Domina i primi 4 secondi (0-4)
    { "Acquire",  Task_HRT_Generic, TASK_TYPE_HRT, 0, 5, 0 },

    // 2. [SRT] Logging: Sempre attivo in background
    { "Filter",  Task_HRT_Generic, TASK_TYPE_HRT, 10, 14, 0 },

    // 3. [HRT] Controllo: Domina la parte centrale (8-12)
    { "Actuate",  Task_HRT_Generic, TASK_TYPE_HRT, 30, 35, 0 },

    // 4. [SRT] Display: Altro background
    { "Logger", Task_SRT_Generic, TASK_TYPE_SRT, 0, 0, 0 },

    // 5. [HRT] Motori: Domina la fine del frame (16-19)
    { "Telemetry", Task_SRT_Generic, TASK_TYPE_SRT, 0, 0, 0 },
    { "Diagnostics", Task_SRT_Generic, TASK_TYPE_SRT, 0, 0, 0 },
};

static TimelineConfig_t my_config = {
    .major_frame_ms = 50,   // 20 Tick totali
    .task_count = 6,        // 5 Task totali
    .tasks = my_tasks
};

const TimelineConfig_t* get_system_configuration(void) {
    return &my_config;
}