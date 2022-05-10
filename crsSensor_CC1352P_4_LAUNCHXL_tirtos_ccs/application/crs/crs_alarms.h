/*
 * crs_alarms.h
 *
 *  Created on: 3 ???? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_CRS_ALARMS_H_
#define APPLICATION_CRS_CRS_ALARMS_H_
#include "application/crs/crs.h"
#include "application/crs/crs_cli.h"

#define ALARMS_NUM 8

typedef enum Alarms_alarmType
{
    DLMaxInputPower,
    ULMaxOutputPower,
    MaxCableLoss,
    SystemTemperature,
    ULMaxInputPower,
    DLMaxOutputPower
} Alarms_alarmType_t;

typedef enum Alarms_alarmMode
{
    ALARM_INACTIVE,
    ALARM_ACTIVE,
    ALARM_STICKY
} Alarms_alarmMode_t;

#define ALARM_ACTIVE_BIT_LOCATION 0
#define ALARM_STICKY_BIT_LOCATION 1



CRS_retVal_t Alarms_init(void *sem);
CRS_retVal_t Alarms_printAlarms();
CRS_retVal_t Alarms_process(void);
CRS_retVal_t Alarms_setAlarm(Alarms_alarmType_t alarmType);

CRS_retVal_t Alarms_clearAlarm(Alarms_alarmType_t alarmType,
                               Alarms_alarmMode_t alarmMode);
CRS_retVal_t Alarms_getTemperature(int16_t *currentTemperature);
CRS_retVal_t Alarms_setTemperatureHigh(int16_t temperature);

CRS_retVal_t Alarms_setTemperatureLow(int16_t temperature);
CRS_retVal_t Alarms_temp_Init();
CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg);


#endif /* APPLICATION_CRS_CRS_ALARMS_H_ */