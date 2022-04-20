/*
 * crs_tx.c
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_tx.h"
#include <ti/sysbios/knl/Semaphore.h>

/* EasyLink API Header files */
#include "easylink/EasyLink.h"
#include "mac/mac_util.h"



/******************************************************************************
 Constants and definitions
 *****************************************************************************/


/******************************************************************************
 Local variables
 *****************************************************************************/

void* sem;

static EasyLink_Status gStatus;

//static EasyLink_TxDoneCb gCbTxFailed = NULL;
//static EasyLink_TxDoneCb gCbCcaFailed = NULL;
//static EasyLink_TxDoneCb gCbSuccess = NULL;

static EasyLink_TxDoneCb gCbTx = NULL;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void txDoneCb(EasyLink_Status status);



void TX_init(void * semaphore)
{
    sem = semaphore;
}


//typedef struct Frame
//{
//    uint16_t len;
//    uint16_t seqSent;
//    uint16_t seqRcv;
//    uint8_t srcAddr[8];
//    uint8_t dstAddr[8];
//    MAC_commandId_t commandId;
//    uint8_t isNeedAck;
//} MAC_crsPacket_t;

//typedef struct
//{
//        uint8_t dstAddr[8];              //!<  Destination address
//        uint32_t absTime;                //!< Absolute time to Tx packet (0 for immediate)
//                                         //!< Layer will use last SeqNum used + 1
//        uint8_t len;                     //!< Payload Length
//        uint8_t payload[EASYLINK_MAX_DATA_LENGTH];       //!< Payload
//} EasyLink_TxPacket;
//void TX_sendPacket(MAC_crsPacket_t* pkt, EasyLink_TxDoneCb cbTxFailed, EasyLink_TxDoneCb cbCcaFailed, EasyLink_TxDoneCb cbSuccess)

void TX_sendPacket(MAC_crsPacket_t* pkt, EasyLink_TxDoneCb cbTx)
{
//    gCbTxFailed = cbTxFailed;
//    gCbCcaFailed = cbCcaFailed;
//    gCbSuccess = cbSuccess;

    if (cbTx != NULL)
    {
        gCbTx = cbTx;
    }
    else
    {
        gCbTx = txDoneCb;
    }
    EasyLink_TxPacket txPacket = { { 0 }, 0, 0, { 0 } };

    uint8_t *pBuf = txPacket.payload;
    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);
    pBuf = Util_bufferUint16(pBuf, pkt->seqRcv);


    memcpy(pBuf, pkt->srcAddr, 8);
    pBuf = pBuf + 8;

    memcpy(pBuf, pkt->dstAddr, 8);
    pBuf = pBuf + 8;

    *pBuf = (uint8_t) pkt->isNeedAck;
    pBuf++;
    *pBuf = (uint8_t) pkt->commandId;

    pBuf++;

    memcpy(pBuf, pkt->payload, pkt->len);


    txPacket.len = (pBuf - txPacket.payload) + pkt->len;
    memcpy(txPacket.dstAddr, pkt->dstAddr, 8);
    /*
     * Address filtering is enabled by default on the Rx device with the
     * an address of 0xAA. This device must set the dstAddr accordingly.
     */
//    txPacket.dstAddr[0] = CRS_PAN_ID;

//    txPacket.dstAddr[0] = 0xaa;


    EasyLink_transmitCcaAsync(&txPacket, gCbTx);


}

void TX_sendPacketBuf(uint8_t* pBuf, uint16_t len, uint8_t *dstAddr, EasyLink_TxDoneCb cbTx)
{


    if (cbTx != NULL)
    {
        gCbTx = cbTx;
    }
    else
    {
        gCbTx = txDoneCb;
    }
    EasyLink_TxPacket txPacket = { { 0 }, 0, 0, { 0 } };



    memcpy(txPacket.payload, pBuf, len);

    txPacket.len = len;
    if (dstAddr != NULL)
    {
        memcpy(txPacket.dstAddr, dstAddr, 8);
    }
    /*
     * Address filtering is enabled by default on the Rx device with the
     * an address of 0xAA. This device must set the dstAddr accordingly.
     */
//    txPacket.dstAddr[0] = CRS_PAN_ID;

//    txPacket.dstAddr[0] = 0xaa;


    EasyLink_transmitCcaAsync(&txPacket, gCbTx);


}


void TX_buildBufFromSrct(MAC_crsPacket_t* pkt, uint8_t *pBuf)
{
    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);
    pBuf = Util_bufferUint16(pBuf, pkt->seqRcv);

    memcpy(pBuf, pkt->srcAddr, 8);
    pBuf = pBuf + 8;

    memcpy(pBuf, pkt->dstAddr, 8);
    pBuf = pBuf + 8;

    *pBuf = (uint8_t) pkt->isNeedAck;
    pBuf++;
    *pBuf = (uint8_t) pkt->commandId;

    pBuf++;

    memcpy(pBuf, pkt->payload, pkt->len);
}

void TX_getPcktStatus(EasyLink_Status* status)
{
    *status = gStatus;
}

static void txDoneCb(EasyLink_Status status)
{
//    gCbTxFailed = cbTxFailed;
//    gCbCcaFailed = cbCcaFailed;
//    gCbSuccess = cbSuccess;
    gStatus = status;

    if (status == EasyLink_Status_Success)
    {
//        gCbSuccess();

    }
    else if(status == EasyLink_Status_Aborted)
    {
//        gCbTxFailed();

    }
    else
    {
//        gCbCcaFailed();

    }

    Util_setEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
    Semaphore_post(sem);
}
