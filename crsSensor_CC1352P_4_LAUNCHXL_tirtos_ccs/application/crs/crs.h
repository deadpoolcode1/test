/*
 * crs.h
 *
 *  Created on: 22 Nov 2021
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_H_
#define APPLICATION_CRS_CRS_H_
//#define CLI_SENSOR
#include <stdint.h>
#include <ti/drivers/Temperature.h>
#include "crs_global_defines.h"

typedef enum CRS_retVal
{
    CRS_SUCCESS,
    CRS_FAILURE,
    CRS_INVALID_CB,
    CRS_RESOURCE_ALREADY_ACQUIRED,
    CRS_RESOURCE_NOT_ACQUIRED,
    CRS_MODULE_UNINITIALIZED,
    CRS_INVALID_CLIENT_HANDLE,
    CRS_MAX_CLIENTS_REACHED,
    CRS_NO_ASYNC_LINES_RELEASED,
    CRS_INVALID_LINE_ID,
    CRS_UNKOWN_VALUE_TYPE,
    CRS_UART_FAILURE,
    CRS_INVALID_PARAM,
    CRS_MAX_MENUS_REACHED,
    CRS_PREV_WRITE_UNFINISHED,
    CRS_MISSING_UART_UPDATE_FN,
    CRS_NOT_MANAGING_UART,
    CRS_LAST_ELEMENT,
    CRS_SHORT_ADDR_NOT_VALID,
    CRS_NEXT_LINE,
    CRS_NO_RSP,
    CRS_EOF,
    CRS_TDD_NOT_OPEN,
    CRS_TDD_NOT_LOCKED
} CRS_retVal_t;

typedef enum CRS_chipMode
{
   MODE_NATIVE,
   MODE_SPI_SLAVE
} CRS_chipMode_t;

typedef enum CRS_chipType
{
   DIG,
   RF,
   UNKNOWN,
   RCM11,
   RCM12,
   RCM21,
   RCM22
} CRS_chipType_t;

#define NAMEVALUE_NAME_SZ 30
#define NAME_VALUES_SZ 10
#define ALARMS_NUM 8
typedef struct CRS_nameValue
{
    char name[NAMEVALUE_NAME_SZ];
    int32_t value;
} CRS_nameValue_t;

typedef enum CRS_alarmType
{
    DLMaxInputPower,
    ULMaxOutputPower,
    MaxCableLoss,
    SystemTemperature,
    ULMaxInputPower,
    DLMaxOutputPower
} CRS_alarmType_t;

typedef enum CRS_alarmMode
{
    ALARM_INACTIVE,
    ALARM_ACTIVE,
    ALARM_STICKY
} CRS_alarmMode_t;

#define ALARM_ACTIVE_BIT_LOCATION 0
#define ALARM_STICKY_BIT_LOCATION 1


void CRS_setTemperatureHigh(int16_t temperature);
void CRS_setTemperatureLow(int16_t temperature);

void CRS_printAlarms();
CRS_retVal_t CRS_setAlarm(CRS_alarmType_t alarmType);
CRS_retVal_t CRS_clearAlarm(CRS_alarmType_t alarmType, CRS_alarmMode_t alarmMode);

void *CRS_malloc(uint16_t size);
void CRS_free(void *ptr);
void CRS_getTemperature(int16_t* currentTemperature);
void CRS_init();

#endif /* APPLICATION_CRS_CRS_H_ */
