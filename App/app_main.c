#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"
#include "uart.h"

static void prvTimelineMonitorTask( void * pvArg );

int main( void )
{
    UART_init();

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
    UART_puts( "[TL-LEGEND] F=frame-start R=release C=complete M=deadline-miss X=context-switch @tick=kernel-tick\r\n" );

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
            uint32_t ulSubframeCount = 0U;
            uint32_t ulSfIdx;

            if( ( gTimelineConfig.ulSubframeMs > 0U ) &&
                ( gTimelineConfig.ulMajorFrameMs >= gTimelineConfig.ulSubframeMs ) )
            {
                ulSubframeCount = gTimelineConfig.ulMajorFrameMs / gTimelineConfig.ulSubframeMs;
            }

            UART_puts( "[TL-SEQ] frame=" );
            UART_put_u32( ulLastCompleteFrameId );

            for( ulSfIdx = 0U; ulSfIdx < ulSubframeCount; ulSfIdx++ )
            {
                BaseType_t xHasTaskInSubframe = pdFALSE;

                UART_puts( " | sf" );
                UART_put_u32( ulSfIdx );
                UART_putc( ':' );

                for( ulEvtIdx = 0U; ulEvtIdx < ulLastCompleteFrameCount; ulEvtIdx++ )
                {
                    const TimelineTraceEvent_t * pxEvt = &xLastCompleteFrameEvents[ulEvtIdx];
                    const char * pcTaskName = "-";
                    char cEvent = '?';

                    if( pxEvt->ulSubframeId != ulSfIdx )
                    {
                        continue;
                    }

                    if( ( gTimelineConfig.pxTasks != NULL ) && ( pxEvt->uxTaskIndex < gTimelineConfig.ulTaskCount ) )
                    {
                        pcTaskName = gTimelineConfig.pxTasks[pxEvt->uxTaskIndex].pcName;
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
                        case TIMELINE_TRACE_EVT_CONTEXT_SWITCH:
                            cEvent = 'X';
                            break;
                        default:
                            cEvent = '?';
                            break;
                    }

                    if( pxEvt->xType == TIMELINE_TRACE_EVT_FRAME_START )
                    {
                        UART_puts( " F" );
                        UART_putc( '@' );
                        UART_put_u32( ( uint32_t ) pxEvt->xTick );
                    }
                    else
                    {
                        UART_putc( ' ' );
                        UART_puts( pcTaskName );
                        UART_putc( ':' );
                        UART_putc( cEvent );
                        UART_putc( '@' );
                        UART_put_u32( ( uint32_t ) pxEvt->xTick );
                    }

                    xHasTaskInSubframe = pdTRUE;
                }

                if( xHasTaskInSubframe == pdFALSE )
                {
                    UART_puts( " no task in subframe" );
                    UART_put_u32( ulSfIdx );
                }
            }
            UART_puts( "\r\n" );
        }

        vTaskDelay( pdMS_TO_TICKS( 100U ) );
    }
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
