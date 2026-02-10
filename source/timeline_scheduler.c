#include "timeline_scheduler.h"
#include "timeline_config.h"
#include "utils.h"
#include <stdbool.h>

static TaskHandle_t *xTaskHandles = NULL;
static const TimelineConfig_t *pxConfig;

// Variabili per gestire il Round-Robin degli SRT
static TaskHandle_t xSRTHandlesList[10]; // Array di comodo per gli SRT
static int iSRTCount = 0;
static int iSRTNextIndex = 0; // Chi tocca adesso?

// --- STAMPA CONFIGURAZIONE ---
static void vPrintSystemConfig(void) {
    UART_printf("\n==========================================\n");
    UART_printf("   TIMELINE CONFIGURATION\n");
    UART_printf("==========================================\n");
    UART_printf("Tasks: "); UART_print_int(pxConfig->task_count); UART_printf("\n\n");
}

static void vSchedulerOrchestrator(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); 
    uint32_t ulCurrentTimeInFrame = 0;

    xLastWakeTime = xTaskGetTickCount();
    UART_printf("[SYSTEM] SCHEDULER STARTED\n");

    for(;;) {
        ulCurrentTimeInFrame = ((xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000) % pxConfig->major_frame_ms;

        // --- NEW FRAME (Reset) ---
        if (ulCurrentTimeInFrame == 0) {
            UART_printf("\n--- NEW FRAME ---\n");
            
            iSRTCount = 0;
            iSRTNextIndex = 0; // Reset del turno all'inizio del frame

            // 1. Ricrea tutti i task e popoliamo la lista SRT separata
            for (uint32_t i = 0; i < pxConfig->task_count; i++) {
                // Se era un SRT vecchio, cancellalo
                if (pxConfig->tasks[i].type == TASK_TYPE_SRT) {
                    if (xTaskHandles[i] != NULL) vTaskDelete(xTaskHandles[i]);
                    
                    // Crea nuovo SRT
                    xTaskCreate(pxConfig->tasks[i].function, pxConfig->tasks[i].task_name, 
                                configMINIMAL_STACK_SIZE, NULL, 1, &xTaskHandles[i]);
                    
                    // Sospendilo subito! Lo attiveremo noi a turno.
                    vTaskSuspend(xTaskHandles[i]);

                    // Salvalo nella lista SRT per gestirlo facilmente
                    xSRTHandlesList[iSRTCount] = xTaskHandles[i];
                    iSRTCount++;
                }
            }
        }

        // --- LOGICA DI SCHEDULING ---
        const char *currentTaskName = "IDLE";
        bool is_hrt_active = false;
        bool deadline_hit = false;

        // 1. GESTIONE HRT (Priorità Assoluta)
        for (uint32_t i = 0; i < pxConfig->task_count; i++) {
            TimelineTaskConfig_t *t = &pxConfig->tasks[i];

            if (t->type == TASK_TYPE_HRT) {
                // START
                if (ulCurrentTimeInFrame == t->ulStart_time_ms) {
                    if (xTaskHandles[i] == NULL) {
                        xTaskCreate(t->function, t->task_name, configMINIMAL_STACK_SIZE, NULL, 
                                    configMAX_PRIORITIES - 2, &xTaskHandles[i]);
                    }
                }
                // DEADLINE
                if (ulCurrentTimeInFrame == t->ulEnd_time_ms) {
                    if (xTaskHandles[i] != NULL) {
                        vTaskDelete(xTaskHandles[i]);
                        xTaskHandles[i] = NULL;
                        currentTaskName = "-> DEADLINE END <-";
                        deadline_hit = true;
                        is_hrt_active = true; // Tecnicamente è un evento HRT
                    }
                }
                // CHECK ACTIVE
                if (xTaskHandles[i] != NULL && !deadline_hit) {
                    currentTaskName = t->task_name;
                    is_hrt_active = true;
                }
            }
        }

        // 2. GESTIONE SRT (Round Robin Manuale)
        // Se c'è un HRT, sospendi tutti gli SRT.
        // Se NON c'è HRT, sveglia UN solo SRT e sospendi gli altri.
        
        if (is_hrt_active) {
            // HRT Domina: Sospendi tutti gli SRT per sicurezza
            for(int k=0; k<iSRTCount; k++) {
                if(xSRTHandlesList[k] != NULL) vTaskSuspend(xSRTHandlesList[k]);
            }
        } 
        else if (iSRTCount > 0) {
            // Spazio Libero: tocca a un SRT!
            
            // A. Sospendi TUTTI prima (per spegnere quello del tick precedente)
            for(int k=0; k<iSRTCount; k++) {
                 if(xSRTHandlesList[k] != NULL) vTaskSuspend(xSRTHandlesList[k]);
            }

            // B. Sveglia SOLO quello di turno
            TaskHandle_t xTaskToRun = xSRTHandlesList[iSRTNextIndex];
            if (xTaskToRun != NULL) {
                vTaskResume(xTaskToRun);
                
                // Trova il nome per la stampa
                // (Trucco veloce: cerchiamo quale config corrisponde a questo handle)
                for(int j=0; j<pxConfig->task_count; j++) {
                     if(xTaskHandles[j] == xTaskToRun) {
                         currentTaskName = pxConfig->tasks[j].task_name;
                         break;
                     }
                }
            }

            // C. Avanza l'indice per il prossimo tick libero (Round Robin)
            iSRTNextIndex++;
            if (iSRTNextIndex >= iSRTCount) {
                iSRTNextIndex = 0; // Torna al primo
            }
        }

        // --- STAMPA ---
        UART_printf("Tick "); 
        UART_print_int(ulCurrentTimeInFrame);
        UART_printf(": [ "); 
        UART_printf(currentTaskName);
        UART_printf(" ]\n");

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void vStartTimelineScheduler(const TimelineConfig_t *cfg) {
    pxConfig = cfg;
    vPrintSystemConfig();
    xTaskHandles = (TaskHandle_t *) pvPortMalloc(sizeof(TaskHandle_t) * cfg->task_count);
    for (uint32_t i = 0; i < cfg->task_count; i++) xTaskHandles[i] = NULL;
    
    xTaskCreate(vSchedulerOrchestrator, "Orchestrator", configMINIMAL_STACK_SIZE * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    vTaskStartScheduler();
}

uint32_t ulGetTimelineCurrentTime(void) {
    return ((xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000);
}