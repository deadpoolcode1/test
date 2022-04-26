/*
 * crs_rx.c
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_rx.h"
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

void *sem;

static EasyLink_Status gStatus;
//static EasyLink_RxPacket  gRxPacket = {0};

static EasyLink_ReceiveCb gCbRx = NULL;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void rxDoneCb(EasyLink_RxPacket *rxPacket, EasyLink_Status status);

void RX_init(void *semaphore)
{
    sem = semaphore;
}

void RX_enterRx(EasyLink_ReceiveCb cbRx, uint8_t dstAddr[8])
{
    if (dstAddr == NULL)
    {
        EasyLink_enableRxAddrFilter(NULL, 8, 1);
    }
    else
    {
//        EasyLink_enableRxAddrFilter(NULL, 8, 1);

        EasyLink_enableRxAddrFilter(dstAddr, 8, 1);

    }
    if (cbRx == NULL)
    {
        gCbRx = rxDoneCb;
    }
    else
    {
        gCbRx = cbRx;
    }

    EasyLink_receiveAsync(gCbRx, 0);
}

void RX_getPacket(MAC_crsPacket_t *pkt)
{
//    CP_LOG(CP_CLI_DEBUG, "RECIVED A PACKET");
//    uint8_t* pBuf = gRxPacket.payload;
//    pkt->seqSent = Util_buildUint16(*pBuf, *(pBuf + 1));
//    pBuf++;
//    pBuf++;
//
//    pkt->seqRcv = Util_buildUint16(*pBuf, *(pBuf + 1));
//    pBuf++;
//    pBuf++;
//
//    memcpy(pkt->srcAddr, pBuf, 8);
//    pBuf = pBuf + 8;
//
//    memcpy(pkt->dstAddr, pBuf, 8);
//    pBuf = pBuf + 8;
//
//    pkt->isNeedAck = *pBuf;
//
//    pBuf++;
//
//    pkt->commandId = (MAC_commandId_t)*pBuf;

}

void RX_buildStructPacket(MAC_crsPacket_t *pkt, uint8_t *pcktBuff)
{
    uint8_t *pBuf = pcktBuff;
    pkt->seqSent = Util_buildUint16(*pBuf, *(pBuf + 1));
    pBuf++;
    pBuf++;

    pkt->seqRcv = Util_buildUint16(*pBuf, *(pBuf + 1));
    pBuf++;
    pBuf++;

    memcpy(pkt->srcAddr, pBuf, 8);
    pBuf = pBuf + 8;

    memcpy(pkt->dstAddr, pBuf, 8);
    pBuf = pBuf + 8;

    pkt->isNeedAck = *pBuf;

    pBuf++;

    pkt->commandId = (MAC_commandId_t) *pBuf;

    pBuf++;

    pkt->len = Util_buildUint16(*pBuf, *(pBuf + 1));
    pBuf++;
    pBuf++;

    memcpy(pkt->payload, pBuf, 100);

}

void RX_getPcktStatus(EasyLink_Status *status)
{
    *status = gStatus;
}

static void rxDoneCb(EasyLink_RxPacket *rxPacket, EasyLink_Status status)
{
//    gCbRxFailed = cbRxFailed;
//      gCbSuccess = cbSuccess;
    if (status == EasyLink_Status_Success)
    {
        /* Toggle RLED to indicate RX */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
    }
    else if (status == EasyLink_Status_Aborted)
    {
//        gCbRxFailed();
        /* Toggle GLED to indicate command aborted */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));
    }
    else
    {
//        /* Toggle GLED and RLED to indicate error */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
    }

//    memcpy(&gRxPacket, rxPacket, sizeof(EasyLink_RxPacket));
    gStatus = status;
    Util_setEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
    Semaphore_post(sem);

}
