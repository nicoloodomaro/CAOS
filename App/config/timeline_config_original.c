#include "FreeRTOS.h"
#include "task.h"
#include "timeline_config.h"

#ifndef CONFIG_PROFILE
#define CONFIG_PROFILE    7
#endif

static void vBusyWaitMs(const TimelineTaskExecutionInfo_t * pxExecInfo, uint32_t ulDurationMs)
{
    TickType_t xTargetTicks = pdMS_TO_TICKS(ulDurationMs);

    if ((pxExecInfo == NULL) || (xTargetTicks == 0U)) {
        return;
    }

    while (xTimelineSchedulerGetTaskExecutedTicks(pxExecInfo->uxTaskIndex) < xTargetTicks) {
        taskYIELD();
    }
}

#if (CONFIG_PROFILE == 6)

static void vHrtOtto(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vHrtDodici(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vHrtSette(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vHrtVenti(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 20U);
}

static void vHrtTre(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vSrtTrenta(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 30U);
}

static void vSrtSette(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vSrtQuarantacinque(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 45U);
}

static void vSrtDiciotto(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 18U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_A", vHrtTre,    TIMELINE_TASK_HRT, 0U, 3U, 15U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrtDodici, TIMELINE_TASK_HRT, 0U, 7U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrtSette,  TIMELINE_TASK_HRT, 1U, 5U, 15U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrtVenti,  TIMELINE_TASK_HRT, 3U, 0U, 12U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_E", vHrtOtto,   TIMELINE_TASK_HRT, 7U, 11U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_F", vHrtTre,    TIMELINE_TASK_HRT, 9U, 12U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_A", vSrtTrenta, TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_B", vSrtSette,  TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_C", vSrtQuarantacinque, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_D", vSrtDiciotto,       TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 200U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#elif (CONFIG_PROFILE == 7)

static void vHrtMissA(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 24U);
}

static void vHrtMissB(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 140U);
}

static void vSrtShort(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 5U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_MISS_A",   vHrtMissA,       TIMELINE_TASK_HRT, 0U, 1U, 8U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 1U, 0U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MISS_B",   vHrtMissB,       TIMELINE_TASK_HRT, 1U, 5U, 12U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 15U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_SHORT",    vSrtShort,       TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#elif (CONFIG_PROFILE == 8)

static void vHrt4(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vHrt6(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static void vHrt5(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 5U);
}

static void vHrt7(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static void vSrt12(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vSrt9(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 9U);
}

static void vSrt8(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vSrt6(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_A", vHrt4, TIMELINE_TASK_HRT, 0U, 2U, 14U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrt6, TIMELINE_TASK_HRT, 1U, 4U, 18U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrt5, TIMELINE_TASK_HRT, 3U, 6U, 22U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrt7, TIMELINE_TASK_HRT, 5U, 3U, 19U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_A", vSrt12, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_B", vSrt9,  TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_C", vSrt8,  TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_D", vSrt6,  TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 180U,
    .ulSubframeMs = 30U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#elif (CONFIG_PROFILE == 9)

static void vHrtChain1(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 9U);
}

static void vHrtChain2(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 23U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 130U);
}

static void vSrtShort(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_CH1",      vHrtChain1,      TIMELINE_TASK_HRT, 0U, 0U, 6U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_CH2",      vHrtChain2,      TIMELINE_TASK_HRT, 0U, 6U, 14U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 2U, 2U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 14U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_SHORT",    vSrtShort,       TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#elif (CONFIG_PROFILE == 10)

static void vHrt3(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 3U);
}

static void vHrt6(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 6U);
}

static void vHrt8(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vHrt4(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 4U);
}

static void vSrt15(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 15U);
}

static void vSrt12(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 12U);
}

static void vSrt10(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 10U);
}

static void vSrt9(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 9U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_A", vHrt3, TIMELINE_TASK_HRT, 0U, 1U, 10U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_B", vHrt6, TIMELINE_TASK_HRT, 2U, 3U, 15U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_C", vHrt8, TIMELINE_TASK_HRT, 4U, 5U, 21U, tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_D", vHrt4, TIMELINE_TASK_HRT, 7U, 4U, 16U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_A", vSrt15, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_B", vSrt12, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_C", vSrt10, TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_D", vSrt9,  TIMELINE_TASK_SRT, 0U, 0U, 0U, tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 225U,
    .ulSubframeMs = 25U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#elif (CONFIG_PROFILE == 11)

static void vHrtTightMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 11U);
}

static void vHrtBlockedMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 2U);
}

static void vHrtPassSubframe(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 30U);
}

static void vHrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 8U);
}

static void vSrtMajorMiss(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 200U);
}

static void vSrtAux(void * pvArg)
{
    const TimelineTaskExecutionInfo_t * pxExecInfo = (const TimelineTaskExecutionInfo_t *) pvArg;
    vBusyWaitMs(pxExecInfo, 7U);
}

static const TimelineTaskConfig_t xTasks[] = {
    { "HRT_TIGHT",    vHrtTightMiss,   TIMELINE_TASK_HRT, 1U, 3U, 9U,   tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_BLOCKED",  vHrtBlockedMiss, TIMELINE_TASK_HRT, 1U, 4U, 11U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_PASS_SF",  vHrtPassSubframe,TIMELINE_TASK_HRT, 3U, 0U, 20U,  tskIDLE_PRIORITY + 4U, 256U },
    { "HRT_MAJ_MISS", vHrtMajorMiss,   TIMELINE_TASK_HRT, 5U, 16U, 20U, tskIDLE_PRIORITY + 4U, 256U },
    { "SRT_MAJ_MISS", vSrtMajorMiss,   TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U },
    { "SRT_AUX",      vSrtAux,         TIMELINE_TASK_SRT, 0U, 0U, 0U,   tskIDLE_PRIORITY + 1U, 256U }
};

const TimelineConfig_t gTimelineConfig = {
    .ulMajorFrameMs = 120U,
    .ulSubframeMs = 20U,
    .pxTasks = xTasks,
    .ulTaskCount = (uint32_t) (sizeof(xTasks) / sizeof(xTasks[0]))
};

#else
#error "Unsupported CONFIG_PROFILE value. Use 6, 7, 8, 9, 10, or 11."
#endif
