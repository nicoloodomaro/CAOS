#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"

#include <stdint.h>
#include <stdio.h>

#define UART0_ADDRESS    ( 0x40004000UL )
#define UART0_CTRL       ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) )
#define UART0_BAUDDIV    ( *( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) )

static void prvTimelineMonitorTask( void * pvArg );
static void prvUARTInit( void );

int main( void )
{
    prvUARTInit();

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

static void prvTimelineMonitorTask( void * pvArg )
{
    static TimelineTraceEvent_t xEvents[64];
    static TimelineTraceEvent_t xFrameBuilderEvents[64];
    static TimelineTraceEvent_t xLastCompleteFrameEvents[64];
    static uint32_t ulFrameBuilderId = 0xFFFFFFFFU;
    static uint32_t ulFrameBuilderCount = 0U;
    static uint32_t ulLastCompleteFrameId = 0xFFFFFFFFU;
    static uint32_t ulLastCompleteFrameCount = 0U;

    ( void ) pvArg;

    vTaskDelay( pdMS_TO_TICKS( 100U ) );

    for( ; ; )
    {
        uint32_t ulEventCount;
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

        vTaskDelay( pdMS_TO_TICKS( 100U ) );
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
