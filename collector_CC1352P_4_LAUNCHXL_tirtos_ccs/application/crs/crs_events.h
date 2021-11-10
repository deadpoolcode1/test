/*
 * crs_events.h
 *
 *  Created on: 3 Nov 2021
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_EVENTS_H_
#define APPLICATION_CRS_CRS_EVENTS_H_

#include <stdbool.h>
#include <stdint.h>


typedef uint32_t CRS_clientHandle_t;

typedef struct {
    bool manageUart;
} CRS_params_t;

typedef struct {
    char clientName[MAX_CLIENT_NAME_LEN];
    uint8_t maxStatusLines;
} CRS_clientParams_t;

typedef struct CRS_eventsData {
   uint32_t  eventId;
   uint8_t  eventType;
   uint32_t  eventTime;//---NEED TO CHECK HOW MUCH BITS ARE NEDDED!   uint32_t time= Timestamp_get32();
   uint32_t  managedObjectClass;
   uint16_t  managedObjectInstance;
   uint8_t  perceivedSeverity;
   uint32_t  additionalInformation[3];
} CRS_eventsData_t;

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
    CRS_SHORT_ADDR_NOT_VALID
} CRS_retVal_t;



CRS_retVal_t  Crs_init();

CRS_clientHandle_t Crs_clientOpen(CRS_clientParams_t* _pParams);

CRS_retVal_t Crs_clientClose(const CRS_clientHandle_t _clientHandle);

CRS_retVal_t Crs_getEvent(const CRS_clientHandle_t _clientHandle, CRS_eventsData_t * retData );

void Crs_addEvent(uint8_t  eventType, uint32_t  managedObjectClass, uint16_t  managedObjectInstance, uint8_t  perceivedSeverity, uint32_t  additionalInformation[3]);


#endif /* APPLICATION_CRS_CRS_EVENTS_H_ */
