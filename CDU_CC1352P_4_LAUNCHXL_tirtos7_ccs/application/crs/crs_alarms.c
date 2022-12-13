/*
 * crs_alarms.c
 *
 *  Created on: 3 ???? 2022
 *      Author: nizan
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_fpga_uart.h"
#include "crs_alarms.h"
#include "crs_nvs.h"
#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>
#include "mac/mac_util.h"
#include <ti/drivers/GPIO.h>
#include <ti/sysbios/knl/Clock.h>
#include "crs_global_defines.h"
#include "application/agc/agc.h"
#ifndef CLI_SENSOR
#include "application/collector.h"
#endif
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ALARMS_SET_TEMP_HIGH_ALARM_EVT 0x0001
#define ALARMS_SET_TEMP_LOW_ALARM_EVT 0x0002
#define ALARMS_SET_TDDLOCK_ALARM_EVT 0x0004
#define ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT 0x0008
#define ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT 0x00010
#define ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT 0x0020
#define ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT 0x0040

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define TEMP_SZ 200
#define EXPECTEDVAL_SZ 20

/******************************************************************************
 Local variables
 *****************************************************************************/
static uint8_t gAlarmArr[ALARMS_NUM];
static Temperature_NotifyObj gNotifyObject;
static Semaphore_Handle collectorSem;
static uint16_t Alarms_events = 0;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Alarms_printAlarms()
{
    CLI_cliPrintf("\r\n0x1 DLMaxInputPower 0x%x", gAlarmArr[DLMaxInputPower]);
    CLI_cliPrintf("\r\n0x2 ULMaxOutputPower 0x%x", gAlarmArr[ULMaxOutputPower]);
    CLI_cliPrintf("\r\n0x3 MaxCableLoss 0x%x", gAlarmArr[MaxCableLoss]);
    CLI_cliPrintf("\r\n0x4 SystemTemperatureHigh 0x%x",
                  gAlarmArr[SystemTemperatureHigh]);
    CLI_cliPrintf("\r\n0x5 SystemTemperatureLow 0x%x",
                  gAlarmArr[SystemTemperatureLow]);
    CLI_cliPrintf("\r\n0x6 ULMaxInputPower 0x%x", gAlarmArr[ULMaxInputPower]);
    CLI_cliPrintf("\r\n0x7 DLMaxOutputPower 0x%x", gAlarmArr[DLMaxOutputPower]);
    CLI_cliPrintf("\r\n0x8 TDDLock 0x%x", gAlarmArr[TDDLock]);
    CLI_cliPrintf("\r\n0x9 PLLLockPrimary 0x%x", gAlarmArr[PLLLockPrimary]);
    CLI_cliPrintf("\r\n0xa PLLLockSecondary 0x%x", gAlarmArr[PLLLockSecondary]);
    CLI_cliPrintf("\r\n0xb SyncPLLLock 0x%x", gAlarmArr[SyncPLLLock]);
return CRS_SUCCESS;
}
/**
 * turns on the alarm_active bit or the failed to discover bit
 */
CRS_retVal_t Alarms_setAlarm(Alarms_alarmType_t alarmType)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }
//    if (alarmType == PLLLockPrimary && (gIsPllPrimaryDiscoverd == false))
//    {
//        gAlarmArr[alarmType] |= 1UL << ALARM_DISCOVERY_BIT_LOCATION; //turn on the failed discovery bit
//        return CRS_SUCCESS;
//    }
//    if (alarmType == PLLLockSecondary && (gIsPllsecondaryDiscoverd == false))
//    {
//        gAlarmArr[alarmType] |= 1UL << ALARM_DISCOVERY_BIT_LOCATION; //turn on the failed discovery bit
//        return CRS_SUCCESS;
//    }
    gAlarmArr[alarmType] |= 1UL << ALARM_ACTIVE_BIT_LOCATION; //turn on the alarm active bit
    gAlarmArr[alarmType] |= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
    return CRS_SUCCESS;
}

/**
 * @brief clears an alarm or clears a sticky mode.
 * alarmMode : ALARM_INACTIVE | ALARM_STICKY
 */
CRS_retVal_t Alarms_clearAlarm(Alarms_alarmType_t alarmType,
                               Alarms_alarmMode_t alarmMode)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }

    if (alarmMode == ALARM_INACTIVE)
    {
        //move to sticky state if before was in active
        if ((gAlarmArr[alarmType] & 1) == ALARM_ACTIVE)
        {
            gAlarmArr[alarmType] |= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
        }
        gAlarmArr[alarmType] &= ~(1UL << ALARM_ACTIVE_BIT_LOCATION); //turn off the alarm active bit
        if (alarmType == SystemTemperatureHigh)
        {
            char envFile[4096] = { 0 };
            Thresh_read("UpperTempThr", envFile);
            int16_t highTempThrsh = strtol(envFile + strlen("UpperTempThr="),
            NULL,
                                           10);
            memset(envFile, 0, 4096);
            Thresh_read("TempOffset", envFile);
            int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
            NULL,
                                        10);
            CRS_retVal_t status = Alarms_setTemperatureHigh(
                    highTempThrsh + tempOffset);

        }
        else if (alarmType == SystemTemperatureLow)
        {
            char envFile[4096] = { 0 };
            Thresh_read("LowerTempThr", envFile);
            int16_t lowTempThrsh = strtol(envFile + strlen("LowerTempThr="),
            NULL,
                                           10);
            memset(envFile, 0, 4096);
            Thresh_read("TempOffset", envFile);
            int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
            NULL,
                                        10);
            CRS_retVal_t status = Alarms_setTemperatureLow(
                    lowTempThrsh + tempOffset);
        }
        return CRS_SUCCESS;
    }
    //clear sticky alarm
    else if (alarmMode == ALARM_STICKY)
    {
        gAlarmArr[alarmType] &= ~(1UL << ALARM_STICKY_BIT_LOCATION); //turn off the alarm sticky bit
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

CRS_retVal_t Alarms_process(void)
{
    if (Alarms_events & ALARMS_SET_TEMP_HIGH_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperatureHigh);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_HIGH_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_TEMP_LOW_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperatureLow);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_LOW_ALARM_EVT);
    }

    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_getTemperature(int16_t *currentTemperature)
{
    *currentTemperature = Temperature_getTemperature();
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_setTemperatureHigh(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyHigh(
            &gNotifyObject, temperature, Alarms_tempThresholdHighNotifyFxn,
            NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_setTemperatureLow(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyLow(
            &gNotifyObject, temperature, Alarms_tempThresholdLowNotifyFxn,
            NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

/*!
 *  @brief This function attaches the collector semaphore .
 *
 */
CRS_retVal_t Alarms_init(void *sem)
{
    collectorSem = sem;
    Alarms_temp_Init();
    return CRS_SUCCESS;
}

/*!
 *  @brief This function initializes the Temperature driver, and sets an threshold for it .
 *
 */
CRS_retVal_t Alarms_temp_Init()
{
    Temperature_init();
    char envFile[4096] = { 0 };
    //System Temperature : ID=4, thrshenv= tmp
    Thresh_read("UpperTempThr", envFile);
    int16_t highTempThrsh = strtol(envFile + strlen("UpperTempThr="),
    NULL, 10);
    memset(envFile, 0, 4096);
    Thresh_read("TempOffset", envFile);
    int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
    NULL, 10);
    CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh + tempOffset);
    if (status == CRS_FAILURE)
    {
        return status;
    }
    memset(envFile, 0, 4096);
    Thresh_read("LowerTempThr", envFile);
    int16_t lowTempThrsh = strtol(envFile + strlen("LowerTempThr="),
    NULL, 10);
    status = Alarms_setTemperatureLow(lowTempThrsh + tempOffset);

    return status;

}


CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg)
{

    char envFile[4096] = { 0 };
    //Max Cable Loss: ID=3, thrshenv= MaxCableLoss
//    memcpy(envFile, "MaxCableLoss", strlen("MaxCableLoss"));
    Thresh_read("MaxCableLossThr", envFile);
    uint32_t MaxCableLossThr = strtol(envFile + strlen("MaxCableLossThr="),
    NULL,
                                      10);
    memset(envFile, 0, 4096);
    Thresh_read("ModemTxPwr", envFile);
    uint32_t ModemTxPwr = strtol(envFile + strlen("ModemTxPwr="), NULL, 10);
    memset(envFile, 0, 4096);
    Thresh_read("CblCompFctr", envFile);
    uint32_t CblCompFctr = strtol(envFile + strlen("CblCompFctr="), NULL, 10);
    if ((ModemTxPwr - (rssiAvg) - CblCompFctr) > MaxCableLossThr)
    {
        Alarms_setAlarm(MaxCableLoss);
    }
    else
    {
        Alarms_clearAlarm(MaxCableLoss, ALARM_INACTIVE);
    }
    return CRS_SUCCESS;
}

void Alarms_tempThresholdHighNotifyFxn(int16_t currentTemperature,
                                       int16_t thresholdTemperature,
                                       uintptr_t clientArg,
                                       Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_HIGH_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

void Alarms_tempThresholdLowNotifyFxn(int16_t currentTemperature,
                                      int16_t thresholdTemperature,
                                      uintptr_t clientArg,
                                      Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_LOW_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}



CRS_retVal_t Alarms_stopPooling()
{
    Clock_stop(gClkHandle);
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_startPooling()
{
    Clock_start(gClkHandle);
    return CRS_SUCCESS;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/
