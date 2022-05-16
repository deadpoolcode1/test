/*
 * crs.h
 *
 *  Created on: 22 Nov 2021
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_H_
#define APPLICATION_CRS_CRS_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdint.h>
#include <ti/drivers/Temperature.h>
#include <ti/drivers/PIN.h>
#include "ti_drivers_config.h"

#include "crs_global_defines.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define NAMEVALUE_NAME_SZ 30
#define NAME_VALUES_SZ 10
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
    MODE_NATIVE, MODE_SPI_SLAVE
} CRS_chipMode_t;

typedef enum CRS_chipType
{
    DIG, RF, UNKNOWN, RCM11, RCM12, RCM21, RCM22
} CRS_chipType_t;

typedef struct CRS_nameValue
{
    char name[NAMEVALUE_NAME_SZ];
    int32_t value;
} CRS_nameValue_t;

extern PIN_Handle Crs_pinHandle;


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void* CRS_malloc(uint16_t size);
void CRS_free(void *ptr);
void CRS_init(PIN_Handle pinHandle);

#endif /* APPLICATION_CRS_CRS_H_ */
