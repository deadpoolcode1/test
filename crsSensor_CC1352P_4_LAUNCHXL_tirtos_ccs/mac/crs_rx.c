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

#include "collectorLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/


/******************************************************************************
 Local variables
 *****************************************************************************/

void* sem;

static EasyLink_Status gStatus;
static EasyLink_RxPacket  gRxPacket = {0};



static EasyLink_ReceiveCb gCbRx = NULL;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void rxDoneCb(EasyLink_RxPacket * rxPacket, EasyLink_Status status);

void RX_init(void * semaphore)
{
    sem = semaphore;
}

void RX_enterRx(uint8_t dstMac[8],EasyLink_ReceiveCb cbRx)
{
    if (dstMac==NULL) {
        //when the addrTable is NULL it disables addrFiltering and accepts packets from everyone
        EasyLink_enableRxAddrFilter(NULL,8,1);
    }else{
        EasyLink_enableRxAddrFilter(dstMac,8,1);
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

void RX_getPacket(MAC_crsPacket_t* pkt )
{
    CP_LOG(CP_CLI_DEBUG, "RECIVED A PACKET");
    uint8_t* pBuf = gRxPacket.payload;
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

    pkt->commandId = (MAC_commandId_t)*pBuf;

}

void RX_getPcktStatus(EasyLink_Status* status)
{
    *status = gStatus;
}

void RX_buildStructPacket(MAC_crsPacket_t* pkt, uint8_t *pcktBuff )
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
}


static void rxDoneCb(EasyLink_RxPacket * rxPacket, EasyLink_Status status)
{
//    gCbRxFailed = cbRxFailed;
//      gCbSuccess = cbSuccess;
    if (status == EasyLink_Status_Success)
    {
        /* Toggle RLED to indicate RX */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
    }
    else if(status == EasyLink_Status_Aborted)
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

    memcpy(&gRxPacket, rxPacket, sizeof(EasyLink_RxPacket));
    gStatus = status;
    Util_setEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
    Semaphore_post(sem);

}


 void rxDoneCbAckSend(EasyLink_RxPacket * rxPacket, EasyLink_Status status){
//    if (status == EasyLink_Status_Success)
//    {
//    }
//    else if(status == EasyLink_Status_Aborted)
//    {
////        gCbRxFailed();
//    }
//    else
//    {
////        /* Toggle GLED and RLED to indicate error */
//    }
    memcpy(&gRxPacket, rxPacket, sizeof(EasyLink_RxPacket));
        gStatus = status;

        MAC_crsPacket_t pkt = {0};
        uint8_t tmp[8] = {0xcf, 0x26, 0xf4, 0x14, 0x4b, 0x12, 0x00, 0x00};
        pkt.commandId=MAC_COMMAND_ACK;
        TX_sendPacket(&pkt, tmp,NULL);
        Util_setEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
        CollectorLink_collectorLinkInfo_t collectorNode;
        CollectorLink_getCollector(&collectorNode);
        CollectorLink_setTimeout((Clock_FuncPtr )contentProcessCb, 50000/ Clock_tickPeriod);
        CollectorLink_startTimer();
        Semaphore_post(sem);
}




 void rxDoneCbAckReceived(EasyLink_RxPacket * rxPacket, EasyLink_Status status){
//    if (status == EasyLink_Status_Success)
//    {
//    }
//    else if(status == EasyLink_Status_Aborted)
//    {
////        gCbRxFailed();
//    }
//    else
//    {
////        /* Toggle GLED and RLED to indicate error */
//    }
    memcpy(&gRxPacket, rxPacket, sizeof(EasyLink_RxPacket));
        gStatus = status;
        MAC_crsPacket_t* ptr=rxPacket->payload;
//        if (ptr->commandId==MAC_COMMAND_ACK) {
            Util_setEvent(&macEvents, MAC_TASK_ACK_RECEIVED);
//        }
//            Node_nodeInfo_t collectorNode;
//            CollectorLink_getCollector(&collectorNode);
//            CollectorLink_setTimeout((Clock_FuncPtr )contentProcessCb, 50000/ Clock_tickPeriod);
//            CollectorLink_startTimer();
        Semaphore_post(sem);
}


 void contentProcessCb(){
     Util_setEvent(&macEvents, MAC_TASK_CONTENT_READY);
     Semaphore_post(sem);
 }

