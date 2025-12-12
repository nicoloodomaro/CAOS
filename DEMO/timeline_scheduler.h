#ifndef __PROJECT_SCHEDULER__
#define __PROJECT_SCHEDULER__

#include "FreeRTOS.h"
#include "task.h"

typedef enum{
	TASK_TYPE_HARD_RT,
	TASK_TYPE_SOFT_RT
}TaskType_t;

typedef struct{
	const char* task_name; 			//nome del task
	TaskFunction_t function; 		//funzione del task
	TaskType_t type; 				//tipo di task (HRT o SRT)
	
	/*parametri di scheduling HRT*/
	uint32_t ulSubframe_id;         //id del subframe in cui il task deve essere eseguito
	uint32_t ulStart_time_ms;       //tempo di inizio di esecuzione del task all'interno del subframe
	uint32_t ulEnd_time_ms;         //tempo di fine di esecuzione del task all'interno del subframe
	
}TimelineTaskConfig_t;

typedef struct {
    uint32_t ulMajorFrameTicks;     // Durata del major frame (es. 1000 ms)
    uint32_t ulSubFrameTicks;       // Durata del subframe (es. 100 ms)
    TimelineTaskConfig_t *pxTasks;  // Puntatore all'array dei task
    uint8_t ucNumTasks;             // Numero di task nella timeline
} TimelineConfig_t;

// Funzione per configurare lo scheduler con la timeline specificata (da chiamare nel main) 
BaseType_t vConfigureScheduler(const TimelineConfig_t *pxCfg);

// Funzione per validare la configurazione della timeline
BaseType_t xValidateSchedule(const TimelineConfig_t *pxCfg);

// Puntatore alla configurazione attiva dello scheduler (serve a Nico)
extern const TimelineConfig_t *pxActiveSchedule;


#endif