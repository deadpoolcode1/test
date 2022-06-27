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

#include "api_mac.h"
#include "mediator.h"

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
Semaphore_Struct appSem; /* not static so you can see in ROV */
static Semaphore_Handle appSemHandle;

/*! Storage for Events flags */
uint32_t appEvents = 0;

/*! MAC callback table, initialized to no callback table */
static ApiMac_callbacks_t *pMacCallbacks = (ApiMac_callbacks_t*) NULL;

uint8_t appTaskId;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void processIncomingMsg(Mediator_msgObjSentToApp_t *pMsg);
static void deallocateIncomingMsg(Mediator_msgObjSentToApp_t *pMsg);
static void copyMacAddrToApiMacAddr(ApiMac_sAddr_t *pDst, sAddr_t *pSrc);
static void copyDataInd(ApiMac_mcpsDataInd_t *pDst, macMcpsDataInd_t *pSrc);

/******************************************************************************
 Public Functions
 *****************************************************************************/

/*!
 Initialize this module.

 Public function defined in api_mac.h
 */
void* ApiMac_init()
{
    Semaphore_Params semParam;

    Semaphore_Params_init(&semParam);
    semParam.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&appSem, 0, &semParam);
    appSemHandle = Semaphore_handle(&appSem);
    Mediator_setAppSem(appSemHandle);

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
//    macCbackEvent_t *pMsg;

    /* Wait for response message */
    if (Semaphore_pend(appSemHandle, BIOS_WAIT_FOREVER))
    {
        Mediator_msgObjSentToApp_t msg = { 0 };
        bool rsp = Mediator_getNextMacMsg(&msg);
        //check if there is a msg
        if (rsp == false)
        {

        }
        else
        {
            processIncomingMsg(&msg);
            deallocateIncomingMsg(&msg);
        }
//        /* Retrieve the response message */
//        if( (pMsg = (macCbackEvent_t*) OsalPort_msgReceive( appTaskId )) != NULL)
//        {
//            /* Process the message from the MAC stack */
//            processIncomingICallMsg(pMsg);
//        }
//
//        if(pMsg != NULL)
//        {
//            OsalPort_msgDeallocate((uint8_t*)pMsg);
//        }
    }
}

static void processIncomingMsg(Mediator_msgObjSentToApp_t *pMsg)
{
    if (pMacCallbacks != NULL)
    {
        switch (pMsg->msg->hdr.event)
        {
        case MAC_MLME_ASSOCIATE_IND:
            if (pMacCallbacks->pAssocIndCb)
            {
                /* Indication structure */
                ApiMac_mlmeAssociateInd_t ind;

                /* Initialize the structure */
                memset(&ind, 0, sizeof(ApiMac_mlmeAssociateInd_t));

                /* copy the message to the indication structure */
                memcpy(ind.deviceAddress, pMsg->msg->associateInd.deviceAddress,
                       sizeof(ApiMac_sAddrExt_t));
                ind.shortAddr = pMsg->msg->associateInd.shortAddr;
                ind.givenShortAddr = pMsg->msg->associateInd.givenShortAddr;

                free(pMsg->msg);

                /* Initiate the callback */
                pMacCallbacks->pAssocIndCb(&ind);
            }
            break;
        case MAC_MLME_DISASSOCIATE_IND:
            if (pMacCallbacks->pDisassociateIndCb)
            {
                /* Indication structure */
                ApiMac_mlmeDisassociateInd_t ind;

                /* Initialize the structure */
                memset(&ind, 0, sizeof(ApiMac_mlmeDisassociateInd_t));

                /* copy the message to the indication structure */
                memcpy(ind.deviceAddress,
                       pMsg->msg->disassociateInd.deviceAddress,
                       sizeof(ApiMac_sAddrExt_t));

                ind.shortAddr = pMsg->msg->disassociateInd.shortAddr;
                free(pMsg->msg);
                /* Initiate the callback */
                pMacCallbacks->pDisassociateIndCb(&ind);
            }
            break;

        case MAC_MLME_DISCOVERY_IND:
            if (pMacCallbacks->pDiscIndCb)
            {
                /* Indication structure */
                ApiMac_mlmeDiscoveryInd_t ind;

                /* Initialize the structure */
                memset(&ind, 0, sizeof(ApiMac_mlmeDiscoveryInd_t));

                /* copy the message to the indication structure */
                memcpy(ind.deviceAddress, pMsg->msg->discoveryInd.deviceAddress,
                       sizeof(ApiMac_sAddrExt_t));
                ind.shortAddr = pMsg->msg->discoveryInd.shortAddr;
                ind.isLocked = pMsg->msg->discoveryInd.isLocked;

                ind.rssi = pMsg->msg->discoveryInd.rssi;
                ind.rssiRemote.rssiAvg = pMsg->msg->discoveryInd.rssiRemote.rssiAvg;
                ind.rssiRemote.rssiLast = pMsg->msg->discoveryInd.rssiRemote.rssiLast;
                ind.rssiRemote.rssiMax = pMsg->msg->discoveryInd.rssiRemote.rssiMax;
                ind.rssiRemote.rssiMin = pMsg->msg->discoveryInd.rssiRemote.rssiMin;

                free(pMsg->msg);

                /* Initiate the callback */
                pMacCallbacks->pDiscIndCb(&ind);
            }
            break;

        case MAC_MCPS_DATA_IND:
            if (pMacCallbacks->pDataIndCb)
            {
                /* Indication structure */
                ApiMac_mcpsDataInd_t ind;

                /* copy structure to structure */
                copyDataInd(&ind, &(pMsg->msg->dataInd));

                /* Initiate the callback */
                pMacCallbacks->pDataIndCb(&ind);

                free(pMsg->msg);
                free(pMsg->msg->dataInd.msdu.p);
                //TODO: deallocate
            }
            break;

        case MAC_MCPS_DATA_CNF:
            if (pMacCallbacks->pDataCnfCb)
            {
                ApiMac_mcpsDataCnf_t cnf;

                /* Initialize the structure */
                memset(&cnf, 0, sizeof(ApiMac_mcpsDataCnf_t));

                /* copy the message to the confirmation structure */
                cnf.status = (ApiMac_status_t) pMsg->msg->dataCnf.hdr.status;
                cnf.msduHandle = pMsg->msg->dataCnf.msduHandle;
                cnf.timestamp = pMsg->msg->dataCnf.timestamp;
                cnf.timestamp2 = pMsg->msg->dataCnf.timestamp2;
                cnf.retries = pMsg->msg->dataCnf.retries;
                cnf.mpduLinkQuality = pMsg->msg->dataCnf.mpduLinkQuality;
                cnf.correlation = pMsg->msg->dataCnf.correlation;
                cnf.rssi = pMsg->msg->dataCnf.rssi;
                cnf.frameCntr = pMsg->msg->dataCnf.frameCntr;

                /* Initiate the callback */
                pMacCallbacks->pDataCnfCb(&cnf);
                free(pMsg->msg);
                if (pMsg->msg->dataCnf.pDataReq)
                {
                    /* Deallocate the original data request structure */
//                    macMsgDeallocate((macEventHdr_t*) pMsg->msg->dataCnf.pDataReq);
                }
            }
            break;

        }
    }
}

/*!
 * @brief       Copy the MAC data indication to the API MAC data indication
 *
 * @param       pDst - pointer to the API MAC data indication
 * @param       pSrc - pointer to the MAC data indication
 */
static void copyDataInd(ApiMac_mcpsDataInd_t *pDst, macMcpsDataInd_t *pSrc)
{
    /* Initialize the structure */
    memset(pDst, 0, sizeof(ApiMac_mcpsDataInd_t));

    /* copy the message to the indication structure */
    memcpy((pDst->srcDeviceAddress), (pSrc->mac.srcDeviceAddressLong), 8);
    memcpy((pDst->dstDeviceAddress), (pSrc->mac.dstDeviceAddressLong), 8);
    pDst->srcShortAddr = pSrc->mac.srcShortAddr;
    pDst->dstShortAddr = pSrc->mac.dstShortAddr;
    pDst->timestamp = pSrc->mac.timestamp;
    pDst->timestamp2 = pSrc->mac.timestamp2;
    pDst->srcPanId = pSrc->mac.srcPanId;
    pDst->dstPanId = pSrc->mac.dstPanId;
    pDst->mpduLinkQuality = pSrc->mac.mpduLinkQuality;
    pDst->correlation = pSrc->mac.correlation;
    pDst->rssi = pSrc->mac.rssi;
    pDst->dsn = pSrc->mac.dsn;
    pDst->payloadIeLen = pSrc->mac.payloadIeLen;
    pDst->pPayloadIE = pSrc->mac.pPayloadIE;

    pDst->frameCntr = (uint32_t) pSrc->mac.frameCntr;

    /* Copy the payload information */
    pDst->msdu.len = pSrc->msdu.len;
    pDst->msdu.p = pSrc->msdu.p;
}

/*!
 * @brief       Copy the common address type from Mac Stack type to App type.
 *
 * @param       pDst - pointer to the application type
 * @param       pSrc - pointer to the mac stack type
 */
static void copyMacAddrToApiMacAddr(ApiMac_sAddr_t *pDst, sAddr_t *pSrc)
{
    /* Copy each element of the structure */
    pDst->addrMode = (ApiMac_addrType_t) pSrc->addrMode;
    if (pDst->addrMode == ApiMac_addrType_short)
    {
        pDst->addr.shortAddr = pSrc->addr.shortAddr;
    }
    else
    {
        memcpy(pDst->addr.extAddr, pSrc->addr.extAddr,
               sizeof(ApiMac_sAddrExt_t));
    }
}

static void deallocateIncomingMsg(Mediator_msgObjSentToApp_t *pMsg)
{
//    pMsg->msg->
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
extern ApiMac_status_t ApiMac_mcpsDataReq(ApiMac_mcpsDataReq_t *pData)
{
    uint16_t len = pData->msdu.len;

    uint8_t *data = malloc(len + 100);
    memset(data, 0, len + 100);
    memcpy(data, pData->msdu.p, len);

    ApiMac_mcpsDataReq_t *p = malloc(sizeof(ApiMac_mcpsDataReq_t) + 50);
    memset(p, 0, sizeof(ApiMac_mcpsDataReq_t) + 50);
    memcpy(p, pData, sizeof(ApiMac_mcpsDataReq_t));

    p->msdu.p = data;
    Mediator_msgObjSentToMac_t msg = { 0 };
    msg.msg = p;
//    msg.msg->msdu.p = p;
    Mediator_sendMsgToMac(&msg);
}

