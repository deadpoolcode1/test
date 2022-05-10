/*
 * crs_alarms.c
 *
 *  Created on: 3 ???? 2022
 *      Author: cellium
 */

/******************************************************************************
 Includes
 *****************************************************************************/
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

/******************************************************************************
 Local variables
 *****************************************************************************/
static uint8_t gAlarmArr[ALARMS_NUM];
static Temperature_NotifyObj gNotifyObject;
static Semaphore_Handle collectorSem;
static uint16_t Alarms_events = 0;
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ALARMS_SET_TEMP_ALARM_EVT 0x0001

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Alarms_printAlarms()
{
    CLI_cliPrintf("\r\n1 DLMaxInputPower 0x%x", gAlarmArr[DLMaxInputPower]);
    CLI_cliPrintf("\r\n2 ULMaxOutputPower 0x%x", gAlarmArr[ULMaxOutputPower]);
    CLI_cliPrintf("\r\n3 MaxCableLoss 0x%x", gAlarmArr[MaxCableLoss]);
    CLI_cliPrintf("\r\n4 SystemTemperature 0x%x", gAlarmArr[SystemTemperature]);
    CLI_cliPrintf("\r\n5 ULMaxInputPower 0x%x", gAlarmArr[ULMaxInputPower]);
    CLI_cliPrintf("\r\n6 DLMaxOutputPower 0x%x", gAlarmArr[DLMaxOutputPower]);
}
/**
 * turns on the alarm_active bit
 */
CRS_retVal_t Alarms_setAlarm(Alarms_alarmType_t alarmType)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }
    gAlarmArr[alarmType] |= 1UL << ALARM_ACTIVE_BIT_LOCATION; //turn on the alarm active bit
    gAlarmArr[alarmType] |= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
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
        if (alarmType == SystemTemperature)
        {
            char envFile[1024] = { 0 };
            //System Temperature : ID=4, thrshenv= tmp
            Thresh_readVarsFile("TmpThr", envFile, 1);
            int16_t highTempThrsh = strtol(envFile + strlen("TmpThr="),
            NULL,
                                           10);
            CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh);

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
    if (Alarms_events & ALARMS_SET_TEMP_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperature);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_ALARM_EVT);
    }

}

CRS_retVal_t Alarms_getTemperature(int16_t *currentTemperature)
{
    *currentTemperature = Temperature_getTemperature();
}

CRS_retVal_t Alarms_setTemperatureHigh(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyHigh(
            &gNotifyObject, temperature, Alarms_tempThresholdNotifyFxn, NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_setTemperatureLow(int16_t temperature)
{
    temperature = Temperature_getTemperature();
}

/*!
 *  @brief This function attaches the collector semaphore .
 *
 */
CRS_retVal_t Alarms_init(void *sem)
{
    collectorSem = sem;
}

/*!
 *  @brief This function initializes the Temperature driver, and sets an threshold for it .
 *
 */
CRS_retVal_t Alarms_temp_Init()
{
    Temperature_init();
    char envFile[1024] = { 0 };
    //System Temperature : ID=4, thrshenv= tmp
    Thresh_readVarsFile("TmpThr", envFile, 1);
    int16_t highTempThrsh = strtol(envFile + strlen("TmpThr="),
    NULL,
                                   10);
    CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh);
    return status;

}

CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg)
{

    char envFile[1024] = { 0 };
    //MaxCableLoss : ID=3, thrshenv= MaxCableLossThr
    Thresh_readVarsFile("MaxCableLossThr", envFile, 1);
    int8_t highRssiThrsh = strtol(envFile + strlen("MaxCableLossThr="),
    NULL,
                                  10);
    if (rssiAvg > highRssiThrsh)
    {
        Alarms_setAlarm(MaxCableLoss);
    }

}

void Alarms_tempThresholdNotifyFxn(int16_t currentTemperature,
                                   int16_t thresholdTemperature,
                                   uintptr_t clientArg,
                                   Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
//    Alarms_setAlarm(SystemTemperature);
}
