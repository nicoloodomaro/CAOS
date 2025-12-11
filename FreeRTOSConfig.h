#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_TRACE_FACILITY                 0
#define configGENERATE_RUN_TIME_STATS            0

#define configUSE_PREEMPTION                     1 //to do preemption with HRT task
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configCPU_CLOCK_HZ                       ( ( unsigned long ) 25000000 )
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 ) //each ms
#define configMINIMAL_STACK_SIZE                 ( ( unsigned short ) 80 )
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 60 * 1024 ) )
#define configMAX_TASK_NAME_LEN                  ( 12 )
#define configUSE_TRACE_FACILITY                 0
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  0
#define configUSE_CO_ROUTINES                    0
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configCHECK_FOR_STACK_OVERFLOW           2 //Cambiato da 0 suggerimento da GEMINI
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_QUEUE_SETS                     1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TIME_SLICING			 0 //Suggerimento GEMINI

#define configMAX_PRIORITIES                     ( 9UL )
#define configMAX_CO_ROUTINE_PRIORITIES          ( 2 )
#define configQUEUE_REGISTRY_SIZE                10
#define configSUPPORT_STATIC_ALLOCATION          0

/* Timer related defines. */
#define configUSE_TIMERS                         0
#define configTIMER_TASK_PRIORITY                ( configMAX_PRIORITIES - 4 )
#define configTIMER_QUEUE_LENGTH                 20
#define configTIMER_TASK_STACK_DEPTH             ( configMINIMAL_STACK_SIZE * 2 )

#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    3

/* Set the following definitions to 1 to include the API function, or zero
 * to exclude the API function. */

#define INCLUDE_vTaskPrioritySet                  1
#define INCLUDE_uxTaskPriorityGet                 1
#define INCLUDE_vTaskDelete                       1
#define INCLUDE_vTaskCleanUpResources             0
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_vTaskDelayUntil                   1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_uxTaskGetStackHighWaterMark       1
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle    1
#define INCLUDE_xTaskGetIdleTaskHandle            1
#define INCLUDE_xSemaphoreGetMutexHolder          1
#define INCLUDE_eTaskGetState                     1
#define INCLUDE_xTimerPendFunctionCall            1
#define INCLUDE_xTaskAbortDelay                   1
#define INCLUDE_xTaskGetHandle                    1
#define INCLUDE_xTaskGetTickCount		  1 //per sapere che ore sono dice GEMINI

/* This demo makes use of one or more example stats formatting functions. These
 * format the raw data provided by the uxTaskGetSystemState() function in to human
 * readable ASCII form.  See the notes in the implementation of vTaskList() within
 * FreeRTOS/Source/tasks.c for limitations. */
#define configUSE_STATS_FORMATTING_FUNCTIONS      0

#define configKERNEL_INTERRUPT_PRIORITY           ( 255 )        /* All eight bits as QEMU doesn't model the priority bits. */

#ifndef __IASMARM__ /* Prevent C code being included in IAR asm files. */
	#define configASSERT( x ) if( ( x ) == 0 ) while(1);
#endif


/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
 * See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY             ( 4 )

/* Use the Cortex-M3 optimised task selection rather than the generic C code
 * version. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION          1

/* The Win32 target is capable of running all the tests tasks at the same
 * time. */
#define configRUN_ADDITIONAL_TESTS                       1

/* The test that checks the trigger level on stream buffers requires an
 * allowable margin of error on slower processors (slower than the Win32
 * machine on which the test is developed). */
#define configSTREAM_BUFFER_TRIGGER_LEVEL_TEST_MARGIN    4

#define intqHIGHER_PRIORITY      ( configMAX_PRIORITIES - 5 )
#define bktPRIMARY_PRIORITY      ( configMAX_PRIORITIES - 3 )
#define bktSECONDARY_PRIORITY    ( configMAX_PRIORITIES - 4 )

#define configENABLE_BACKWARD_COMPATIBILITY 0

#endif /* FREERTOS_CONFIG_H */



