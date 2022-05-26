/*
 * crs_alarms.h
 *
 *  Created on: 3 ???? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_CRS_ALARMS_H_
#define APPLICATION_CRS_CRS_ALARMS_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs.h"
#include "application/crs/crs_cli.h"
#include <ti/sysbios/knl/Clock.h>
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define ALARM_ACTIVE_BIT_LOCATION 0
#define ALARM_STICKY_BIT_LOCATION 1
#define ALARM_DISCOVERY_BIT_LOCATION 2

typedef enum Alarms_alarmType
{
    DLMaxInputPower,
    ULMaxOutputPower,
    MaxCableLoss,
    SystemTemperature,
    ULMaxInputPower,
    DLMaxOutputPower,
    TDDLock,
    PLLLockPrimary,
    PLLLockSecondary,
    SyncPLLLock,
    NumberOfAlarms
} Alarms_alarmType_t;
#define ALARMS_NUM 10
typedef enum Alarms_alarmMode
{
    ALARM_INACTIVE,
    ALARM_ACTIVE,
    ALARM_STICKY
} Alarms_alarmMode_t;




/******************************************************************************
 Function Prototypes
 *****************************************************************************/
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
CRS_retVal_t Alarms_TDDLock_Init();
CRS_retVal_t Alarms_PLL_Check_Clock_Init(Clock_FuncPtr clockFxn);
CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg, uint16_t shortAddr);
void Alarms_tempThresholdNotifyFxn(int16_t currentTemperature,
                                   int16_t thresholdTemperature,
                                   uintptr_t clientArg,
                                   Temperature_NotifyObj *notifyObject);
void Alarms_TDDLockNotifyFxn(uint_least8_t index);
CRS_retVal_t Alarms_stopPooling();
CRS_retVal_t Alarms_startPooling();
CRS_retVal_t Alarms_discoveryPllPrimary();
CRS_retVal_t Alarms_discoveryPllSecondary();
#endif /* APPLICATION_CRS_CRS_ALARMS_H_ */
