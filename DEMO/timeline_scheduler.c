#include "timeline_scheduler.h"
#include <string.h> 


// Il puntatore che Nico userà per leggere la tabella
const TimelineConfig_t *pxActiveSchedule = NULL;

// Array interno per salvare gli handle dei task (esempio 16 ma dobbiamo vedere insieme quante task mettere come massimo)
#define MAX_TIMELINE_TASKS 16
TaskHandle_t xTimelineTaskHandles[MAX_TIMELINE_TASKS];


BaseType_t xValidateSchedule(const TimelineConfig_t *pxCfg) {
    // Controllo sui puntatori
    if (pxCfg == NULL || pxCfg->pxTasks == NULL) {
        return pdFAIL;
    }
    if (pxCfg->ulSubFrameTicks == 0) return pdFAIL;

    // Controllo per validare ogni task
    for (uint8_t i = 0; i < pxCfg->ucNumTasks; i++) {
        TimelineTaskConfig_t *task = &pxCfg->pxTasks[i];

        // lo start time deve essere minore del end time
        if (task->ulStart_time_ms >= task->ulEnd_time_ms) {
            return pdFAIL; 
        }

        // La task non può sforare la durata del subframe
        if (task->ulEnd_time_ms > pxCfg->ulSubFrameTicks) {
            return pdFAIL; 
        }

        // Controllo sul subframe id controllando che non superi il numero massimo di subframe nel major frame
        uint32_t max_subframes = pxCfg->ulMajorFrameTicks / pxCfg->ulSubFrameTicks;
        if (task->ulSubframe_id >= max_subframes) {
            return pdFAIL;
        }
    }
    return pdPASS;
}

BaseType_t vConfigureScheduler(const TimelineConfig_t *pxCfg) {
    // Chiamo la funzione di validazione
    if (xValidateSchedule(pxCfg) != pdPASS) {
        return pdFAIL; 
    }

    // Salvo la configurazione attiva (serve a Nico)
    pxActiveSchedule = pxCfg;

    // Simulazione della creazione dei task
    for (uint8_t i = 0; i < pxCfg->ucNumTasks; i++) {
        
        if (i >= MAX_TIMELINE_TASKS) break; // Protezione array

        TimelineTaskConfig_t *t = &pxCfg->pxTasks[i];
        
        BaseType_t res = xTaskCreate(
            t->function,              // La funzione del task
            t->task_name,             // Nome del task
            configMINIMAL_STACK_SIZE, // <--- Uso stack minimo per ora (IN CHIAMATA DOBBIAMO VALUTARE QUESTO ASPETTO)
            NULL,                     // Nessun parametro per ora
            tskIDLE_PRIORITY,         // Priorità fittizia tanto li sospendiamo al prossimo passo
            &xTimelineTaskHandles[i]  // Salvo l'handle del task (per Nico)
        );

        if (res != pdPASS) {
            return pdFAIL; 
        }

        // Sospendo subito il task, sarà Nico a riattivarlo quando serve
        vTaskSuspend(xTimelineTaskHandles[i]);
    }

    return pdPASS;
}