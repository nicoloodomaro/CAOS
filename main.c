// Application entry point for the timeline-driven scheduler example.

#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"
#include "timeline_scheduler.h"

int main(void)
{
    // Load the timeline configuration (tasks + frame timing).
    vConfigureScheduler(&g_timeline);

    // Create the scheduler task at a higher priority than HRT tasks.
    (void)xStartTimelineScheduler(tskIDLE_PRIORITY + 4, 512);

    // Start the FreeRTOS kernel.
    vTaskStartScheduler();

    // Should never reach here.
    for (;;) { }
}
