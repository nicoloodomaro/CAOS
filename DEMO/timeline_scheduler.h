#ifndef __PROJECT_SCHEDULER__
#define __PROJECT_SCHEDULER__

#include "FreeRTOS.h"

typedef enum{
	TASK_TYPE_HART_RT,
	TASK_TYPE_SOFT_RT
}TaskType_t;

typedef struct{
	const char* task_name;
	TaskFunction_t function;
	TaskType_t type;
	
	/*parametri di scheduling HRT*/
	uint32_t ulSubframe_id;
	uint32_t ulStart_time_ms;
	uint32_t ulEnd_time_ms;
	
	/*opzionale*/
	uint16_t usStackDepth;
}TimelineTaskConfig_t





#endif
