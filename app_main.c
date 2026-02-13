#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"

#include <stdint.h>
#include <stdio.h>

#define UART0_ADDRESS    ( 0x40004000UL )
#define UART0_CTRL       ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) )
#define UART0_BAUDDIV    ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) )

static void prvHeartbeatTask( void * pvArg );
static void prvTimelineMonitorTask( void * pvArg );
static void prvUARTInit( void );

int main( void )
{
    prvUARTInit();

    /* A visible task to prove the scheduler is alive together with timeline tasks. */
    ( void ) xTaskCreate( prvHeartbeatTask,
                          "HB",
                          configMINIMAL_STACK_SIZE * 2U,
                          NULL,
                          tskIDLE_PRIORITY + 1U,
                          NULL );
    ( void ) xTaskCreate( prvTimelineMonitorTask,
                          "TL_MON",
                          configMINIMAL_STACK_SIZE * 6U,
                          NULL,
                          tskIDLE_PRIORITY + 1U,
                          NULL );

    vTaskStartScheduler();

    for( ; ; )
    {
    }
}

static void prvHeartbeatTask( void * pvArg )
{
    ( void ) pvArg;

    for( ; ; )
    {
        printf( "[HB] tick=%u\r\n", ( unsigned int ) xTaskGetTickCount() );
        vTaskDelay( pdMS_TO_TICKS( 1000U ) );
    }
}

static void prvTimelineMonitorTask( void * pvArg )
{
    static TimelineTraceEvent_t xEvents[64];
    static TimelineTraceEvent_t xFrameBuilderEvents[64];
    static TimelineTraceEvent_t xLastCompleteFrameEvents[64];
    static uint32_t ulPrevCompletion[TIMELINE_MAX_TASKS];
    static uint32_t ulPrevRelease[TIMELINE_MAX_TASKS];
    static uint32_t ulPrevMiss[TIMELINE_MAX_TASKS];
    static uint32_t ulFrameBuilderId = 0xFFFFFFFFU;
    static uint32_t ulFrameBuilderCount = 0U;
    static uint32_t ulLastCompleteFrameId = 0xFFFFFFFFU;
    static uint32_t ulLastCompleteFrameCount = 0U;

    ( void ) pvArg;

    vTaskDelay( pdMS_TO_TICKS( 1200U ) );

    for( ; ; )
    {
        uint32_t ulEventCount;
        uint32_t ulTaskCount = 0U;
        const TimelineTaskRuntime_t * pxRt = pxTimelineSchedulerGetRuntime( &ulTaskCount );
        uint32_t ulIdx;
        uint32_t ulEvtIdx;

        do
        {
            ulEventCount = ulTimelineSchedulerTraceRead( &xEvents[0], 64U );
            for( ulEvtIdx = 0U; ulEvtIdx < ulEventCount; ulEvtIdx++ )
            {
                const TimelineTraceEvent_t * pxEvt = &xEvents[ulEvtIdx];

                if( ulFrameBuilderId == 0xFFFFFFFFU )
                {
                    ulFrameBuilderId = pxEvt->ulFrameId;
                }

                if( pxEvt->ulFrameId != ulFrameBuilderId )
                {
                    uint32_t ulCopyIdx;

                    ulLastCompleteFrameId = ulFrameBuilderId;
                    ulLastCompleteFrameCount = ulFrameBuilderCount;
                    for( ulCopyIdx = 0U; ( ulCopyIdx < ulFrameBuilderCount ) && ( ulCopyIdx < 64U ); ulCopyIdx++ )
                    {
                        xLastCompleteFrameEvents[ulCopyIdx] = xFrameBuilderEvents[ulCopyIdx];
                    }

                    ulFrameBuilderId = pxEvt->ulFrameId;
                    ulFrameBuilderCount = 0U;
                }

                if( ulFrameBuilderCount < 64U )
                {
                    xFrameBuilderEvents[ulFrameBuilderCount] = *pxEvt;
                    ulFrameBuilderCount++;
                }
            }
        }
        while( ulEventCount > 0U );

        if( ulLastCompleteFrameCount > 0U )
        {
            uint32_t ulSfPrinted = 0xFFFFFFFFU;
            printf( "[TL-SEQ] frame=%u", ( unsigned int ) ulLastCompleteFrameId );
            for( ulEvtIdx = 0U; ulEvtIdx < ulLastCompleteFrameCount; ulEvtIdx++ )
            {
                const TimelineTraceEvent_t * pxEvt = &xLastCompleteFrameEvents[ulEvtIdx];
                const char * pcTaskName = "-";
                char cEvent = '?';

                if( ( gTimelineConfig.pxTasks != NULL ) && ( pxEvt->uxTaskIndex < gTimelineConfig.ulTaskCount ) )
                {
                    pcTaskName = gTimelineConfig.pxTasks[pxEvt->uxTaskIndex].pcName;
                }

                if( pxEvt->ulSubframeId != ulSfPrinted )
                {
                    ulSfPrinted = pxEvt->ulSubframeId;
                    printf( " | sf%u:", ( unsigned int ) ulSfPrinted );
                }

                switch( pxEvt->xType )
                {
                    case TIMELINE_TRACE_EVT_FRAME_START:
                        cEvent = 'F';
                        break;
                    case TIMELINE_TRACE_EVT_HRT_RELEASE:
                    case TIMELINE_TRACE_EVT_SRT_RELEASE:
                        cEvent = 'R';
                        break;
                    case TIMELINE_TRACE_EVT_TASK_COMPLETE:
                        cEvent = 'C';
                        break;
                    case TIMELINE_TRACE_EVT_DEADLINE_MISS:
                        cEvent = 'M';
                        break;
                    default:
                        cEvent = '?';
                        break;
                }

                if( pxEvt->xType == TIMELINE_TRACE_EVT_FRAME_START )
                {
                    printf( " F");
                }
                else
                {
                    printf( " %s:%c", pcTaskName, cEvent );
                }
            }
            printf( "\r\n" );
        }

        if( ( pxRt != NULL ) && ( ulTaskCount > 0U ) )
        {
            for( ulIdx = 0U; ( ulIdx < ulTaskCount ) && ( ulIdx < TIMELINE_MAX_TASKS ); ulIdx++ )
            {
                const TimelineTaskConfig_t * pxCfg = &gTimelineConfig.pxTasks[ ulIdx ];
                uint32_t ulRelDelta = pxRt[ ulIdx ].ulReleaseCount - ulPrevRelease[ ulIdx ];
                uint32_t ulCmpDelta = pxRt[ ulIdx ].ulCompletionCount - ulPrevCompletion[ ulIdx ];
                uint32_t ulMissDelta = pxRt[ ulIdx ].ulDeadlineMissCount - ulPrevMiss[ ulIdx ];

                if( ( ulRelDelta > 0U ) || ( ulCmpDelta > 0U ) || ( ulMissDelta > 0U ) )
                {
                    printf( "[TL] %s type=%s rel=%u(+%u) cmp=%u(+%u) miss=%u(+%u)\r\n",
                            pxCfg->pcName,
                            ( pxCfg->xType == TIMELINE_TASK_HRT ) ? "HRT" : "SRT",
                            ( unsigned int ) pxRt[ ulIdx ].ulReleaseCount,
                            ( unsigned int ) ulRelDelta,
                            ( unsigned int ) pxRt[ ulIdx ].ulCompletionCount,
                            ( unsigned int ) ulCmpDelta,
                            ( unsigned int ) pxRt[ ulIdx ].ulDeadlineMissCount,
                            ( unsigned int ) ulMissDelta );
                }

                ulPrevRelease[ ulIdx ] = pxRt[ ulIdx ].ulReleaseCount;
                ulPrevCompletion[ ulIdx ] = pxRt[ ulIdx ].ulCompletionCount;
                ulPrevMiss[ ulIdx ] = pxRt[ ulIdx ].ulDeadlineMissCount;
            }
        }

        vTaskDelay( pdMS_TO_TICKS( 1000U ) );
    }
}

static void prvUARTInit( void )
{
    UART0_BAUDDIV = ( ( configCPU_CLOCK_HZ / 38400U ) - 1U );
    UART0_CTRL = 1U;
}

void vApplicationMallocFailedHook( void )
{
    taskDISABLE_INTERRUPTS();
    for( ; ; )
    {
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    ( void ) xTask;
    ( void ) pcTaskName;

    taskDISABLE_INTERRUPTS();
    for( ; ; )
    {
    }
}

void vAssertCalled( const char * pcFileName, uint32_t ulLine )
{
    ( void ) pcFileName;
    ( void ) ulLine;

    taskDISABLE_INTERRUPTS();
    for( ; ; )
    {
    }
}

/* The startup vector references these IRQ handlers. */
void TIMER0_Handler( void )
{
}

void TIMER1_Handler( void )
{
}
