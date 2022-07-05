/*
 * crs.c
 *
 *  Created on: 23 Jan 2022
 *      Author: avi
 */
/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_nvs.h"
#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>
#include <ti/drivers/Watchdog.h>
#include <ti/sysbios/knl/Clock.h>

#include "ti_drivers_config.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
CRS_cb_gain_states_t CRS_cbGainStates;

/******************************************************************************
 Local variables
 *****************************************************************************/

static Watchdog_Handle gWatchdogHandle;
static Watchdog_Params gParams;

static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;

static uint32_t gWatchdogTimeInTicks = 100*1000*5;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void watchdogClearTimeoutCb(xdc_UArg arg);


/******************************************************************************
 Public Functions
 *****************************************************************************/
void CRS_init()
{
    CRS_cbGainStates.dc_rf_high_freq_hb_rx = 21;
    CRS_cbGainStates.dc_if_low_freq_tx = 21;
    CRS_cbGainStates.uc_rf_high_freq_hb_tx = 21;
    CRS_cbGainStates.uc_if_low_freq_rx = 15;

    Watchdog_init();
    /* Open a Watchdog driver instance */
    Watchdog_Params_init(&gParams);
//    params.callbackFxn = (Watchdog_Callback) watchdogCallback;
//    params.debugStallMode = Watchdog_DEBUG_STALL_ON;
//    params.resetMode = Watchdog_RESET_ON;

    gWatchdogHandle = Watchdog_open(CONFIG_WATCHDOG_0, &gParams);
    if (gWatchdogHandle == NULL)
    {
        /* Error opening Watchdog */
        while (1)
        {
        }
    }

    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;

    Clock_construct(&gClkStruct, NULL, 1000000 / Clock_tickPeriod, &gClkParams);

    gClkHandle = Clock_handle(&gClkStruct);

    Clock_setFunc(gClkHandle, watchdogClearTimeoutCb, 0);
    Clock_setTimeout(gClkHandle, gWatchdogTimeInTicks / 2);
    Clock_start(gClkHandle);

}

void* CRS_malloc(uint16_t size)
{
    return malloc(size);

}

void* CRS_calloc(uint16_t num ,size_t size)
{
    return calloc(num, size);

}

void* CRS_realloc(void* ptr, uint16_t size)
{
    return realloc(ptr, size);

}

/*!
 Csf implementation for memory de-allocation

 Public function defined in csf.h
 */
void CRS_free(void *ptr)
{
//    CRS_LOG(CRS_DEBUG, "FREEE");
    if (ptr != NULL)
    {
        free(ptr);

    }
}


CRS_retVal_t CRS_watchdogDisable()
{

    Clock_stop(gClkHandle);
    Watchdog_close(gWatchdogHandle);

}

/******************************************************************************
 Local Functions
 *****************************************************************************/

static void watchdogClearTimeoutCb(xdc_UArg arg)
{
    Watchdog_clear(gWatchdogHandle);

    Clock_setFunc(gClkHandle, watchdogClearTimeoutCb, 0);
    Clock_setTimeout(gClkHandle, gWatchdogTimeInTicks / 2);
    Clock_start(gClkHandle);
}

