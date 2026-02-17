#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"
#include "uart.h"

static void prvTimelineMonitorTask( void * pvArg );
static const char * prvEventTypeToText( TimelineTraceEventType_t xType );

#define TL_MONITOR_READ_BATCH_EVENTS    64U
#define TL_MONITOR_PENDING_EVENTS       256U

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
    static TimelineTraceEvent_t xReadEvents[ TL_MONITOR_READ_BATCH_EVENTS ];
    static TimelineTraceEvent_t xPendingEvents[ TL_MONITOR_PENDING_EVENTS ];
    uint32_t ulPendingEventCount = 0U;
    uint32_t ulDroppedEventCount = 0U;
    BaseType_t xHasNextTick = pdFALSE;
    TickType_t xNextTickToPrint = 0U;
    uint32_t ulCurrentMajorFrameId = 0xFFFFFFFFU;
    TickType_t xCurrentMajorFrameStartTick = 0U;
    uint32_t ulLastPrintedMajorFrameId = 0xFFFFFFFFU;
    uint32_t ulLastPrintedSubframeId = 0xFFFFFFFFU;
    BaseType_t xSimTaskActive[ TIMELINE_MAX_TASKS ] = { 0 };
    int32_t lCurrentRunningTaskIdx = -1;
    const TickType_t xMajorFrameTicks = pdMS_TO_TICKS( gTimelineConfig.ulMajorFrameMs );
    const TickType_t xSubframeTicks = pdMS_TO_TICKS( gTimelineConfig.ulSubframeMs );

    ( void ) pvArg;

    UART_puts( "[TL-LEGEND] Release, Complete, DeadlineMiss, ContextSwitch, FrameStart\r\n" );
    UART_puts( "[TL-NOTE] running derives from ContextSwitch events.\r\n" );

    for( ; ; )
    {
        TickType_t xNowTick;
        TickType_t xTick;
        uint32_t ulReadCount;
        uint32_t ulEvtIdx;

        do
        {
            ulReadCount = ulTimelineSchedulerTraceRead( &xReadEvents[ 0 ], TL_MONITOR_READ_BATCH_EVENTS );
            for( ulEvtIdx = 0U; ulEvtIdx < ulReadCount; ulEvtIdx++ )
            {
                if( ulPendingEventCount < TL_MONITOR_PENDING_EVENTS )
                {
                    xPendingEvents[ ulPendingEventCount ] = xReadEvents[ ulEvtIdx ];
                    ulPendingEventCount++;
                }
                else
                {
                    ulDroppedEventCount++;
                }
            }
        }
        while( ulReadCount == TL_MONITOR_READ_BATCH_EVENTS );

        if( ( xHasNextTick == pdFALSE ) && ( ulPendingEventCount > 0U ) )
        {
            xNextTickToPrint = xPendingEvents[ 0 ].xTick;
            xHasNextTick = pdTRUE;
        }

        if( xHasNextTick == pdFALSE )
        {
            vTaskDelay( pdMS_TO_TICKS( 1U ) );
            continue;
        }

        xNowTick = xTaskGetTickCount();

        for( xTick = xNextTickToPrint; xTick <= xNowTick; xTick++ )
        {
            uint32_t ulTickEventCount = 0U;
            TickType_t xTickInFrame = 0U;
            uint32_t ulSubframeId = 0U;
            BaseType_t xMajorFrameChanged = pdFALSE;
            BaseType_t xSubframeChanged = pdFALSE;
            const char * pcRunningTask = NULL;

            while( ( ulTickEventCount < ulPendingEventCount ) &&
                   ( xPendingEvents[ ulTickEventCount ].xTick == xTick ) )
            {
                ulTickEventCount++;
            }

            for( ulEvtIdx = 0U; ulEvtIdx < ulTickEventCount; ulEvtIdx++ )
            {
                if( xPendingEvents[ ulEvtIdx ].xType == TIMELINE_TRACE_EVT_FRAME_START )
                {
                    ulCurrentMajorFrameId = xPendingEvents[ ulEvtIdx ].ulFrameId;
                    xCurrentMajorFrameStartTick = xPendingEvents[ ulEvtIdx ].xTick;
                }
            }

            if( ( ulCurrentMajorFrameId == 0xFFFFFFFFU ) &&
                ( ulTickEventCount > 0U ) )
            {
                TickType_t xEstimatedDeltaTicks = 0U;

                ulCurrentMajorFrameId = xPendingEvents[ 0 ].ulFrameId;
                if( xSubframeTicks > 0U )
                {
                    xEstimatedDeltaTicks = ( TickType_t ) xPendingEvents[ 0 ].ulSubframeId * xSubframeTicks;
                }

                if( xEstimatedDeltaTicks <= xTick )
                {
                    xCurrentMajorFrameStartTick = xTick - xEstimatedDeltaTicks;
                }
                else
                {
                    xCurrentMajorFrameStartTick = xTick;
                }
            }

            if( ( ulCurrentMajorFrameId != 0xFFFFFFFFU ) &&
                ( xMajorFrameTicks > 0U ) )
            {
                while( xTick >= ( xCurrentMajorFrameStartTick + xMajorFrameTicks ) )
                {
                    ulCurrentMajorFrameId++;
                    xCurrentMajorFrameStartTick += xMajorFrameTicks;
                }

                xTickInFrame = xTick - xCurrentMajorFrameStartTick;
            }

            if( xSubframeTicks > 0U )
            {
                ulSubframeId = ( uint32_t ) ( xTickInFrame / xSubframeTicks );
            }

            if( ulCurrentMajorFrameId != ulLastPrintedMajorFrameId )
            {
                xMajorFrameChanged = pdTRUE;
            }
            if( ulSubframeId != ulLastPrintedSubframeId )
            {
                xSubframeChanged = pdTRUE;
            }

            UART_puts( "tick " );
            UART_put_u32( ( uint32_t ) xTick );
            UART_puts( "\r\n" );

            UART_puts( "    major-frame = " );
            if( ulCurrentMajorFrameId == 0xFFFFFFFFU )
            {
                UART_puts( "?" );
            }
            else
            {
                UART_put_u32( ulCurrentMajorFrameId );
            }
            UART_puts( "\r\n" );

            UART_puts( "    subframe = " );
            UART_put_u32( ulSubframeId );
            UART_puts( "\r\n" );

            if( ( xMajorFrameChanged != pdFALSE ) ||
                ( xSubframeChanged != pdFALSE ) )
            {
                UART_puts( "    transition = " );
                if( xMajorFrameChanged != pdFALSE )
                {
                    UART_puts( "major-frame change" );
                }
                else
                {
                    UART_puts( "subframe change" );
                }
                UART_puts( "\r\n" );
            }

            if( ulTickEventCount == 0U )
            {
                UART_puts( "    events: none\r\n" );
            }
            else
            {
                for( ulEvtIdx = 0U; ulEvtIdx < ulTickEventCount; ulEvtIdx++ )
                {
                    const TimelineTraceEvent_t * pxEvt = &xPendingEvents[ ulEvtIdx ];
                    const char * pcTaskName = "-";
                    UBaseType_t uxTaskIndex = pxEvt->uxTaskIndex;
                    BaseType_t xTaskIndexValid = pdFALSE;

                    if( uxTaskIndex < TIMELINE_MAX_TASKS )
                    {
                        xTaskIndexValid = pdTRUE;
                    }

                    UART_puts( "    " );
                    if( pxEvt->xType == TIMELINE_TRACE_EVT_FRAME_START )
                    {
                        uint32_t ulResetIdx;

                        for( ulResetIdx = 0U; ulResetIdx < TIMELINE_MAX_TASKS; ulResetIdx++ )
                        {
                            xSimTaskActive[ ulResetIdx ] = pdFALSE;
                        }

                        lCurrentRunningTaskIdx = -1;
                        UART_puts( "FRAME_START\r\n" );
                        continue;
                    }

                    if( ( gTimelineConfig.pxTasks != NULL ) &&
                        ( pxEvt->uxTaskIndex < gTimelineConfig.ulTaskCount ) )
                    {
                        pcTaskName = gTimelineConfig.pxTasks[ pxEvt->uxTaskIndex ].pcName;
                    }
                    else if( pxEvt->xType == TIMELINE_TRACE_EVT_CONTEXT_SWITCH )
                    {
                        pcTaskName = "KERNEL/NON-MANAGED";
                    }

                    UART_puts( pcTaskName );
                    UART_puts( " : " );
                    UART_puts( prvEventTypeToText( pxEvt->xType ) );
                    UART_puts( "\r\n" );

                    if( xTaskIndexValid != pdFALSE )
                    {
                        if( ( pxEvt->xType == TIMELINE_TRACE_EVT_HRT_RELEASE ) ||
                            ( pxEvt->xType == TIMELINE_TRACE_EVT_SRT_RELEASE ) )
                        {
                            xSimTaskActive[ uxTaskIndex ] = pdTRUE;
                        }
                        else if( pxEvt->xType == TIMELINE_TRACE_EVT_CONTEXT_SWITCH )
                        {
                            xSimTaskActive[ uxTaskIndex ] = pdTRUE;
                            lCurrentRunningTaskIdx = ( int32_t ) uxTaskIndex;
                        }
                        else if( ( pxEvt->xType == TIMELINE_TRACE_EVT_TASK_COMPLETE ) ||
                                 ( pxEvt->xType == TIMELINE_TRACE_EVT_DEADLINE_MISS ) )
                        {
                            xSimTaskActive[ uxTaskIndex ] = pdFALSE;
                            if( lCurrentRunningTaskIdx == ( int32_t ) uxTaskIndex )
                            {
                                lCurrentRunningTaskIdx = -1;
                            }
                        }
                    }
                    else if( pxEvt->xType == TIMELINE_TRACE_EVT_CONTEXT_SWITCH )
                    {
                        lCurrentRunningTaskIdx = -1;
                    }
                }
            }

            if( ( lCurrentRunningTaskIdx >= 0 ) &&
                ( ( uint32_t ) lCurrentRunningTaskIdx < gTimelineConfig.ulTaskCount ) &&
                ( ( uint32_t ) lCurrentRunningTaskIdx < TIMELINE_MAX_TASKS ) &&
                ( xSimTaskActive[ ( uint32_t ) lCurrentRunningTaskIdx ] != pdFALSE ) &&
                ( gTimelineConfig.pxTasks != NULL ) &&
                ( gTimelineConfig.pxTasks[ ( uint32_t ) lCurrentRunningTaskIdx ].pcName != NULL ) )
            {
                pcRunningTask = gTimelineConfig.pxTasks[ ( uint32_t ) lCurrentRunningTaskIdx ].pcName;
            }
            else
            {
                const char * pcFirstActive = NULL;
                uint32_t ulActiveIdx;

                for( ulActiveIdx = 0U;
                     ( ulActiveIdx < gTimelineConfig.ulTaskCount ) &&
                     ( ulActiveIdx < TIMELINE_MAX_TASKS );
                     ulActiveIdx++ )
                {
                    if( xSimTaskActive[ ulActiveIdx ] == pdFALSE )
                    {
                        continue;
                    }

                    if( ( gTimelineConfig.pxTasks != NULL ) &&
                        ( gTimelineConfig.pxTasks[ ulActiveIdx ].pcName != NULL ) )
                    {
                        pcFirstActive = gTimelineConfig.pxTasks[ ulActiveIdx ].pcName;
                        break;
                    }
                }

                if( pcFirstActive != NULL )
                {
                    pcRunningTask = "unknown (ready-no-switch)";
                }
            }

            UART_puts( "    running = " );
            if( pcRunningTask != NULL )
            {
                UART_puts( pcRunningTask );
            }
            else
            {
                UART_puts( "idle" );
            }
            UART_puts( "\r\n\r\n" );

            if( ulTickEventCount > 0U )
            {
                uint32_t ulLeftCount = ulPendingEventCount - ulTickEventCount;
                for( ulEvtIdx = 0U; ulEvtIdx < ulLeftCount; ulEvtIdx++ )
                {
                    xPendingEvents[ ulEvtIdx ] = xPendingEvents[ ulEvtIdx + ulTickEventCount ];
                }
                ulPendingEventCount = ulLeftCount;
            }

            ulLastPrintedMajorFrameId = ulCurrentMajorFrameId;
            ulLastPrintedSubframeId = ulSubframeId;
        }

        if( xNowTick < ( TickType_t ) 0xFFFFFFFFU )
        {
            xNextTickToPrint = xNowTick + 1U;
        }
        else
        {
            xNextTickToPrint = xNowTick;
        }

        if( ulDroppedEventCount > 0U )
        {
            UART_puts( "[TL-WARN] dropped_events=" );
            UART_put_u32( ulDroppedEventCount );
            UART_puts( "\r\n" );
            ulDroppedEventCount = 0U;
        }

        vTaskDelay( pdMS_TO_TICKS( 1U ) );
    }
}

static const char * prvEventTypeToText( TimelineTraceEventType_t xType )
{
    switch( xType )
    {
        case TIMELINE_TRACE_EVT_HRT_RELEASE:
        case TIMELINE_TRACE_EVT_SRT_RELEASE:
            return "Release";

        case TIMELINE_TRACE_EVT_TASK_COMPLETE:
            return "Complete";

        case TIMELINE_TRACE_EVT_DEADLINE_MISS:
            return "DeadlineMiss";

        case TIMELINE_TRACE_EVT_CONTEXT_SWITCH:
            return "ContextSwitch";

        case TIMELINE_TRACE_EVT_FRAME_START:
            return "FrameStart";

        default:
            return "Unknown";
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
