/*
 * crs.c
 *
 *  Created on: 23 Jan 2022
 *      Author: avi
 */
#include "crs_nvs.h"

#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>



static uint8_t gAlarmArr[ALARMS_NUM];
static Temperature_NotifyObj gNotifyObject;

static void tempThresholdNotifyFxn(int16_t currentTemperature,
                            int16_t thresholdTemperature, uintptr_t clientArg,
                            Temperature_NotifyObj *notifyObject);

void CRS_printAlarms()
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
CRS_retVal_t CRS_setAlarm(CRS_alarmType_t alarmType)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }
    gAlarmArr[alarmType]|= 1UL << ALARM_ACTIVE_BIT_LOCATION; //turn on the alarm active bit
    gAlarmArr[alarmType]|= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
}

/**
 * clears an alarm or clears a sticky mode
 */
CRS_retVal_t CRS_clearAlarm(CRS_alarmType_t alarmType, CRS_alarmMode_t alarmMode)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }

    if (alarmMode == ALARM_INACTIVE)
    {
        //move to sticky state if before was in active
        if ((gAlarmArr[alarmType]&1) == ALARM_ACTIVE)
        {
            gAlarmArr[alarmType]|= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
        }
        gAlarmArr[alarmType] &= ~(1UL << ALARM_ACTIVE_BIT_LOCATION);//turn off the alarm active bit
        if(alarmType==SystemTemperature){
            char envFile[1024]={0};
             //System Temperature : ID=4, thrshenv= tmp
             memcpy(envFile, "tmp", strlen("tmp"));
             Thresh_readVarsFile("tmp", envFile, 1);
             int16_t highTempThrsh = strtol(envFile + strlen("tmp="),
                           NULL, 16);
              CRS_setTemperatureHigh(highTempThrsh);

        }
    }
    //clear sticky alarm
    else if (alarmMode == ALARM_STICKY)
    {
        gAlarmArr[alarmType]&= ~(1UL << ALARM_STICKY_BIT_LOCATION); //turn off the alarm sticky bit
    }


}









static void tempThresholdNotifyFxn(int16_t currentTemperature,
                            int16_t thresholdTemperature, uintptr_t clientArg,
                            Temperature_NotifyObj *notifyObject)
{

    CRS_setAlarm(SystemTemperature);
}

void CRS_init()
{
    Temperature_init();
    char envFile[1024] = { 0 };
    //System Temperature : ID=4, thrshenv= tmp
    memcpy(envFile, "tmp", strlen("tmp"));
    Thresh_readVarsFile("tmp", envFile, 1);
    int16_t highTempThrsh = strtol(envFile + strlen("tmp="),
    NULL, 16);

    Temperature_registerNotifyHigh(&gNotifyObject, highTempThrsh, tempThresholdNotifyFxn,
                                   NULL);

}

void CRS_getTemperature(int16_t *currentTemperature)
{
    *currentTemperature = Temperature_getTemperature();
}

void CRS_setTemperatureHigh(int16_t temperature)
{
    Temperature_registerNotifyHigh(&gNotifyObject, temperature, tempThresholdNotifyFxn,
                                       NULL);
}

void CRS_setTemperatureLow(int16_t temperature)
{
    temperature = Temperature_getTemperature();
}

void* CRS_malloc(uint16_t size)
{
    return malloc(size);

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

