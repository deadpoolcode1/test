/*
 * crs_events.c
 *
 *  Created on: 3 Nov 2021
 *      Author: epc_4
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/utils/Random.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/apps/Button.h>
#include "ti_drivers_config.h"


#include "collector.h"
#include "cllc.h"
#include "csf.h"
#include "cui.h"
#include "crs_events.h"

#include "util_timer.h"

#include <ti/drivers/dpl/SemaphoreP.h>
//#include <xdc/runtime/Timestamp.h>

#ifdef OSAL_PORT2TIRTOS
#include "osal_port.h"
#else
#include "icall.h"
#endif


#define NUM_EVENTS 200


//typedef struct CRS_eventsNode {
//    CRS_eventsData_t* data;
//    struct CRS_eventsNode* next;
//    uint32_t index;
//} CRS_eventsNode_t;

typedef struct
{
    uint32_t index;
    uint32_t nextEventId;

    //CRS_eventsNode_t* curr;
    uint32_t clientHandle;
} CRS_eventsResource_t;

static CRS_eventsResource_t gEventsStructResourcesArray[MAX_CLIENTS];

static CRS_eventsData_t gEventsStructArray[NUM_EVENTS];

static size_t gNumClients = 0;

//static CRS_eventsNode_t* gFirstEventNode;
//static CRS_eventsNode_t* gLastEventNode;

static uint32_t gLastElementIndex = 0;
static uint32_t gFirstElementIndex = 0;

static uint32_t gTotalEventsCounter = 0;


static bool gModuleInitialized = false;

static CRS_clientHandle_t  gClientHandles[MAX_CLIENTS];
//static uint32_t gMaxStatusLines[2];

static SemaphoreP_Params gSemParams;
static SemaphoreP_Handle gClientsSem;
static SemaphoreP_Struct gClientsSemStruct;

static SemaphoreP_Handle gEventStrctSem;
static SemaphoreP_Struct gEventStrctSemStruct;

static void *Crs_malloc(uint16_t size);
static CRS_retVal_t Crs_publicAPIChecks(const CRS_clientHandle_t _clientHandle);
static CRS_retVal_t Crs_validateHandle(const CRS_clientHandle_t _clientHandle);
static int Crs_getClientIndex(const CRS_clientHandle_t _clientHandle);
static CRS_eventsResource_t* Crs_getClientReasource(const CRS_clientHandle_t _clientHandle);
static void Crs_free(void *ptr);
static void Crs_UserTimerCallbackFunction(Timer_Handle handle, int_fast16_t status);
static CRS_retVal_t Crs_copyEvent(CRS_eventsData_t * dst, CRS_eventsData_t * src);
static uint32_t get_random();


static Timer_Handle    gTimerHandle;
static Timer_Params    gTimerParams;
static uint32_t gTimeStamp = 0;

static uint32_t get_random()
{
    uint8_t attempts = 0;
    uint32_t randomNumber;

    attempts = 0;
    do
    {
        randomNumber = Random_getNumber();
        attempts++;
    }
    while (!randomNumber && (attempts <= 5));
    return randomNumber;
}



CRS_retVal_t  Crs_init()
{


     Timer_Params_init(&gTimerParams);
     gTimerParams.periodUnits = Timer_PERIOD_US;
     gTimerParams.period = 100000;
     gTimerParams.timerMode  = Timer_CONTINUOUS_CALLBACK;
     gTimerParams.timerCallback = Crs_UserTimerCallbackFunction;

     gTimerHandle = Timer_open(CONFIG_TIMER_0, &gTimerParams);
     uint32_t status = Timer_start(gTimerHandle);

    if (status == Timer_STATUS_ERROR)
    {
        //Timer_start() failed
        while (1)
            ;
    }

    // Semaphore Setup
    SemaphoreP_Params_init(&gSemParams);
    //set all sems in this module to be binary sems
    gSemParams.mode = SemaphoreP_Mode_BINARY;

    // Client Setup
    {
        gClientsSem = SemaphoreP_construct(&gClientsSemStruct, 1, &gSemParams);

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            /*
             * A client handle of 0 indicates that the client has not been
             * registered.
             */
            gClientHandles[i] = 0U;
        }
    }

    {
        gEventStrctSem = SemaphoreP_construct(&gEventStrctSemStruct, 1,
                                              &gSemParams);

    }

    uint32_t randomNumber;
    if (Random_STATUS_SUCCESS != Random_seedAutomatic())
    {
        return CRS_FAILURE;
    }

    for (int i = 0; i < NUM_EVENTS; i++)
    {
        gEventsStructArray[i].eventId = gTotalEventsCounter;
        gEventsStructArray[i].eventType = get_random();
        gEventsStructArray[i].eventTime = gTotalEventsCounter;

        gEventsStructArray[i].perceivedSeverity = (get_random()) % 8;
        gEventsStructArray[i].managedObjectClass = get_random();
        gEventsStructArray[i].managedObjectInstance = get_random();
        gEventsStructArray[i].additionalInformation[0] = get_random();
        gEventsStructArray[i].additionalInformation[1] = get_random();
        gEventsStructArray[i].additionalInformation[2] = get_random();
//        uint32_t temp = 1;
//        while ( temp != 0)
//        {
//            temp++;
//        }
        if (gLastElementIndex == NUM_EVENTS - 1)
        {
            gLastElementIndex = 0;
        }
        else
        {
            gLastElementIndex++;
        }
        gTotalEventsCounter++;
    }
    gModuleInitialized = true;

//    gFirstEventNode = NULL;
//    gLastEventNode = NULL;
    return CRS_SUCCESS;


}



CRS_clientHandle_t Crs_clientOpen(CRS_clientParams_t* _pParams)
{


    if (!gModuleInitialized)
    {
        return 0U;
    }

    SemaphoreP_pend(gClientsSem, SemaphoreP_WAIT_FOREVER);

    if (gNumClients >= MAX_CLIENTS)
    {
        SemaphoreP_post(gClientsSem);

        return 0U;
    }

    uint32_t randomNumber;
    if (Random_STATUS_SUCCESS != Random_seedAutomatic()) {
        SemaphoreP_post(gClientsSem);

         return 0U;
    }

    uint8_t attempts = 0;
    do
    {
        randomNumber = Random_getNumber();
        attempts++;
    }while(!randomNumber && (attempts <= 5));

    int placeAvialable = 0;
    for (int j = 0; j <= gNumClients; j++)
    {
        if (gClientHandles[j] == 0)
        {
            placeAvialable = j;
        }
    }

    gClientHandles[placeAvialable] = randomNumber;
    //gEventsStructResourcesArray[placeAvialable] = (CRS_eventsResource_t*)Crs_malloc(  sizeof(CRS_eventsResource_t) );
    gEventsStructResourcesArray[placeAvialable].clientHandle = randomNumber;
    gEventsStructResourcesArray[placeAvialable].index = gFirstElementIndex;
    gEventsStructResourcesArray[placeAvialable].nextEventId = gEventsStructArray[gFirstElementIndex].eventId;
//    if (_pParams->maxStatusLines)
//    {
//        gMaxStatusLines[numClients] = _pParams->maxStatusLines;
//        gStatusLineResources[numClients] = malloc(_pParams->maxStatusLines * sizeof(gStatusLineResources[0][0]));
//        if (gStatusLineResources[numClients] == NULL)
//        {
//            return CUI_FAILURE;
//        }
//        memset(gStatusLineResources[numClients], 0, _pParams->maxStatusLines * sizeof(gStatusLineResources[0][0]));
//    }

    gNumClients++;

    SemaphoreP_post(gClientsSem);

    return randomNumber;
}


CRS_retVal_t Crs_clientClose(const CRS_clientHandle_t _clientHandle)
{
    if (!gModuleInitialized)
    {
        return 0U;
    }

    SemaphoreP_pend(gClientsSem, SemaphoreP_WAIT_FOREVER);

    CRS_retVal_t ret = Crs_validateHandle(_clientHandle);
    if (ret != CRS_SUCCESS)
    {
        SemaphoreP_post(gClientsSem);

        return ret;
    }

    CRS_eventsResource_t *clientResource = Crs_getClientReasource(_clientHandle);
    if (clientResource == NULL)
    {
        SemaphoreP_post(gClientsSem);

        return CRS_FAILURE;
    }

    int index  = Crs_getClientIndex(_clientHandle);
    gClientHandles[index] = 0;
    //gEventsStructResourcesArray[index] = NULL;

    //Crs_free(clientResource);
    gNumClients--;
    SemaphoreP_post(gClientsSem);
    return CRS_SUCCESS;

}

static CRS_retVal_t Crs_publicAPIChecks(const CRS_clientHandle_t _clientHandle)
{
    if (!gModuleInitialized)
    {
        return CRS_MODULE_UNINITIALIZED;
    }

    return CUI_validateHandle(_clientHandle);
}

static CRS_retVal_t Crs_validateHandle(const CRS_clientHandle_t _clientHandle)
{
    if (!_clientHandle)
    {
        return CRS_INVALID_CLIENT_HANDLE;
    }

    if (Crs_getClientIndex(_clientHandle) == -1)
    {
        return CRS_INVALID_CLIENT_HANDLE;
    }
    else
    {
        return CRS_SUCCESS;
    }
}


static int Crs_getClientIndex(const CRS_clientHandle_t _clientHandle)
{
    for (uint32_t i = 0; i < MAX_CLIENTS; i++)
    {
        if (_clientHandle == gClientHandles[i])
        {
            return i;
        }
    }
    return -1;
}

static CRS_eventsResource_t *  Crs_getClientReasource(const CRS_clientHandle_t _clientHandle)
{
    int index = Crs_getClientIndex(_clientHandle);
    if (index == -1)
    {
        return NULL;
    }
   return  &(gEventsStructResourcesArray[index]);
}

CRS_retVal_t Crs_getEvent(const CRS_clientHandle_t _clientHandle, CRS_eventsData_t * retData )
{
    SemaphoreP_pend(gEventStrctSem, SemaphoreP_WAIT_FOREVER);

    CRS_retVal_t ret = Crs_validateHandle(_clientHandle);
    if (ret  != CRS_SUCCESS)
    {
        SemaphoreP_post(gEventStrctSem);

        return ret;
    }

    CRS_eventsResource_t * clientResource = Crs_getClientReasource(_clientHandle);
    if (clientResource == NULL)
    {
        SemaphoreP_post(gEventStrctSem);

        return CRS_FAILURE;
    }

    int index = Crs_getClientIndex(_clientHandle);
    if (gEventsStructResourcesArray[index].nextEventId == gTotalEventsCounter)
    {
        SemaphoreP_post(gEventStrctSem);

        return CRS_LAST_ELEMENT;
    }

    if (gTotalEventsCounter - gEventsStructResourcesArray[index].nextEventId >= NUM_EVENTS)
    {
        gEventsStructResourcesArray[index].index = gFirstElementIndex;
        gEventsStructResourcesArray[index].nextEventId = gEventsStructArray[gFirstElementIndex].eventId;
    }

   //gEventsStructResourcesArray[placeAvialable].nextEventId = gEventsStructArray[gFirstElementIndex].eventId;
    //CRS_eventsData_t  currData =  gEventsStructArray[clientResource->index];
    Crs_copyEvent(retData, &(gEventsStructArray[clientResource->index]));

    if (clientResource->index == NUM_EVENTS - 1)
    {
        clientResource->index = 0;
    }
    else
    {
        clientResource->index = clientResource->index + 1;
    }
    gEventsStructResourcesArray[index].nextEventId = gEventsStructResourcesArray[index].nextEventId + 1;

    SemaphoreP_post(gEventStrctSem);

   return CRS_SUCCESS;
}

static CRS_retVal_t Crs_copyEvent(CRS_eventsData_t * dst, CRS_eventsData_t * src)
{
    if (dst == NULL || src == NULL)
    {
        return CRS_FAILURE;
    }

    //dst = (CRS_eventsData_t*)Crs_malloc(  sizeof(CRS_eventsData_t) );

    dst->eventId = src->eventId;
    dst->eventTime = src->eventTime;
    dst->eventType = src->eventType;
    dst->managedObjectClass =  src->managedObjectClass;
    dst->managedObjectInstance = src->managedObjectInstance;
    dst->perceivedSeverity = src->perceivedSeverity;
    int i = 0;
    for (i = 0; i < 3; i++)
    {
        dst->additionalInformation[i] = src->additionalInformation[i];
    }
    return CRS_SUCCESS;

}

static void Crs_UserTimerCallbackFunction(Timer_Handle handle, int_fast16_t status)
{
    gTimeStamp++;
}

void Crs_addEvent(uint8_t  eventType, uint32_t  managedObjectClass, uint16_t  managedObjectInstance, uint8_t  perceivedSeverity, uint32_t  additionalInformation[3])
{
    SemaphoreP_pend(gEventStrctSem, SemaphoreP_WAIT_FOREVER);

    gEventsStructArray[gLastElementIndex].eventId = gTotalEventsCounter;
    gEventsStructArray[gLastElementIndex].eventType = eventType;
    gEventsStructArray[gLastElementIndex].eventTime = gTimeStamp;
    gEventsStructArray[gLastElementIndex].managedObjectClass = managedObjectClass;
    gEventsStructArray[gLastElementIndex].managedObjectInstance = managedObjectInstance;
    gEventsStructArray[gLastElementIndex].perceivedSeverity = perceivedSeverity;
    gEventsStructArray[gLastElementIndex].additionalInformation[0] = additionalInformation[0];
    gEventsStructArray[gLastElementIndex].additionalInformation[1] = additionalInformation[1];
    gEventsStructArray[gLastElementIndex].additionalInformation[2] = additionalInformation[2];

    if (gLastElementIndex == NUM_EVENTS - 1)
    {
        if (gLastElementIndex == gFirstElementIndex)
                {
                    gFirstElementIndex = 0;
                }
        gLastElementIndex = 0;
    }
    else
    {
        if (gLastElementIndex == gFirstElementIndex)
        {
            gFirstElementIndex++;
        }
        gLastElementIndex++;
    }



    gTotalEventsCounter++;

    SemaphoreP_post(gEventStrctSem);

}


static void *Crs_malloc(uint16_t size)
{
#ifdef OSAL_PORT2TIRTOS
    return OsalPort_malloc(size);
#else
    return(ICall_malloc(size));
#endif /* endif for OSAL_PORT2TIRTOS */
}

static void Crs_free(void *ptr)
{
    if(ptr != NULL)
    {
#ifdef OSAL_PORT2TIRTOS
        OsalPort_free(ptr);
#else
        ICall_free(ptr);
#endif /* endif for OSAL_PORT2TIRTOS */
    }
}
