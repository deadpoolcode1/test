/*
 * crs_api_mac.c
 *
 *  Created on: 30 Mar 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "crs_api_mac.h"



#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/BIOS.h>



/*! Capability Information - Device is capable of becoming a PAN coordinator */
#define CAPABLE_PAN_COORD       0x01

/******************************************************************************
 Global variables
 *****************************************************************************/
/*!
 The ApiMac_extAddr is the MAC's IEEE address, setup with the Chip's
 IEEE addresses in main.c
 */
ApiMac_sAddrExt_t ApiMac_extAddr;

/******************************************************************************
 Local variables
 *****************************************************************************/
/*! Semaphore used to post events to the application thread */
Semaphore_Struct appSem;  /* not static so you can see in ROV */
static Semaphore_Handle appSemHandle;

/*! Storage for Events flags */
uint32_t appEvents = 0;

/*! MAC callback table, initialized to no callback table */
static ApiMac_callbacks_t *pMacCallbacks = (ApiMac_callbacks_t *) NULL;

uint8_t appTaskId;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


/******************************************************************************
 Public Functions
 *****************************************************************************/

/*!
 Initialize this module.

 Public function defined in api_mac.h
 */
void *ApiMac_init()
{
    Semaphore_Params semParam;

    Semaphore_Params_init(&semParam);
    semParam.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&appSem, 0, &semParam);
    appSemHandle = Semaphore_handle(&appSem);

    return (appSemHandle);
}

/*!
 Register for MAC callbacks.

 Public function defined in api_mac.h
 */
void ApiMac_registerCallbacks(ApiMac_callbacks_t *pCallbacks)
{
    /* Save the application's callback table */
    pMacCallbacks = pCallbacks;
}


void ApiMac_processIncoming(void)
{
    macCbackEvent_t *pMsg;

    /* Wait for response message */
    if(Semaphore_pend(appSemHandle, BIOS_WAIT_FOREVER ))
    {
        /* Retrieve the response message */
        if( (pMsg = (macCbackEvent_t*) OsalPort_msgReceive( appTaskId )) != NULL)
        {
            /* Process the message from the MAC stack */
            processIncomingICallMsg(pMsg);
        }

        if(pMsg != NULL)
        {
            OsalPort_msgDeallocate((uint8_t*)pMsg);
        }
    }
}

/*!
 * @brief       This function sends application data to the MAC for
 *              transmission in a MAC data frame.
 *              <BR>
 *              The MAC can only buffer a certain number of data request
 *              frames.  When the MAC is congested and cannot accept the data
 *              request it will initiate a callback ([ApiMac_dataCnfFp_t]
 *              (@ref ApiMac_dataCnfFp_t)) with
 *              an overflow status ([ApiMac_status_transactionOverflow]
 *              (@ref ApiMac_status_t)) .  Eventually the MAC will become
 *              uncongested and initiate the callback ([ApiMac_dataCnfFp_t]
 *              (@ref ApiMac_dataCnfFp_t)) for
 *              a buffered request.  At this point the application can attempt
 *              another data request.  Using this scheme, the application can
 *              send data whenever it wants but it must queue data to be resent
 *              if it receives an overflow status.
 *
 * @param       pData - pointer to parameter structure
 *
 * @return      The status of the request, as follows:<BR>
 *              [ApiMac_status_success](@ref ApiMac_status_success)
 *               - Operation successful<BR>
 *              [ApiMac_status_noResources]
 *              (@ref ApiMac_status_noResources) - Resources not available
 */
extern ApiMac_status_t ApiMac_mcpsDataReq(ApiMac_mcpsDataReq_t *pData);



