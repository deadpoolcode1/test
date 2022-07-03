/*
 * sm_content_ack.c
 *
 *  Created on: 13 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "sm_content_ack.h"
#include "mac/node.h"
#include "mac/crs_tx.h"
#include "mac/crs_rx.h"
#include "mac/mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMAC_RECIVE_ACK_MAX_RETRY_EVT 0x0001
#define SMAC_RECIVE_CONTENT_TIMEOUT_EVT 0x0002
#define SMAC_RECIVED_ACK_EVT 0x0004
#define SMAC_RECIVED_CONTENT_EVT 0x0008
#define SMAC_FINISHED_EVT 0x0010
#define SMAC_ERASE_NODE_EVT 0x0020


typedef enum
{
    SMAC_RX_IDLE,
    SMAC_SENT_CONTENT,
    SMAC_SENT_CONTENT_AGAIN,

    SMAC_RECIVED_CONTENT_ACK,
    SMAC_RECIVED_CONTENT,
    SMAC_RECIVED_CONTENT_AGAIN,

    SMAC_FINISHED_SENDING_CONTENT_ACK,
    SMAC_NOTIFY_FAIL,
    SMAC_NOTIFY_SUCCESS,
    SMAC_ERROR
} Mac_smStateCodes_t;

typedef struct StateMachineAckContent
{
    uint8_t nodeMac[8];
    MAC_crsPacket_t pkt;
//    uint8_t contentRsp[256];
    uint16_t contentRspLen;
    uint8_t contentRspRssi;

} Smac_smAckContent_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *macSem = NULL;

static uint16_t smacEvents = 0;

static uint16_t gTotalSmacPackts = 0;

static Smac_smAckContent_t gSmAckContentInfo = { 0 };

static uint8_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;

static uint8_t gMsduHandle = 0;

static bool gIsInTheMiddle = false;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void smacFinishedSendingContentCb(EasyLink_Status status);
static void smacFinishedSendingContentAgainCb(EasyLink_Status status);
static void ackTimeoutCb(void *arg);
static void waitForSensorContentTimeoutCb(void *arg);
static void smacReceivedContentAckCb(EasyLink_RxPacket *rxPacket,
                                     EasyLink_Status status);

static void smacRecivedContentCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status);

static void smacFinishedSendingContentAckCb(EasyLink_Status status);
static void smacFinishedSendingContentAckAgainCb(EasyLink_Status status);
static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);
static void smacEraseNode();


static void smacNotifySuccessCb(EasyLink_Status status);

static void timeoutCb(void *arg);


/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smac_init(void *sem)
{
    macSem = sem;
}

void Smac_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smacEvents & SMAC_RECIVE_ACK_MAX_RETRY_EVT)
    {
        Util_clearEvent(&smacEvents, SMAC_RECIVE_ACK_MAX_RETRY_EVT);
    }

    if (smacEvents & SMAC_RECIVE_CONTENT_TIMEOUT_EVT)
    {
        Util_clearEvent(&smacEvents, SMAC_RECIVE_CONTENT_TIMEOUT_EVT);
    }

    if (smacEvents & SMAC_RECIVED_ACK_EVT)
    {
        macMcpsDataCnf_t rsp = {0};
        MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_success);
        MAC_sendCnfToApp(&rsp);
        Util_clearEvent(&smacEvents, SMAC_RECIVED_ACK_EVT);
    }

    if (smacEvents & SMAC_RECIVED_CONTENT_EVT)
    {
        gTotalSmacPackts++;
        macMcpsDataInd_t rsp = {0};
        MAC_createDataInd(&rsp, &gSmAckContentInfo.pkt, ApiMac_status_success);
        MAC_sendDataIndToApp(&rsp);
        Util_clearEvent(&smacEvents, SMAC_RECIVED_CONTENT_EVT);
    }

    if (smacEvents & SMAC_FINISHED_EVT)
    {
        MAC_moveToRxIdleState();

        Util_clearEvent(&smacEvents, SMAC_FINISHED_EVT);
    }

    if (smacEvents & SMAC_ERASE_NODE_EVT)
    {
        macMcpsDataCnf_t rsp = { 0 };
        MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_noAck);
        MAC_sendCnfToApp(&rsp);

        macMlmeDisassociateInd_t rsp2 = {0};

        Node_nodeInfo_t node = { 0 };

        Node_getNode(gSmAckContentInfo.nodeMac, &node);
        Node_eraseNode( &node);
        MAC_createDisssocInd(&rsp2, gSmAckContentInfo.nodeMac);
        MAC_sendDisassocIndToApp(&rsp2);
        MAC_moveToRxIdleState();

        Util_clearEvent(&smacEvents, SMAC_ERASE_NODE_EVT);
    }
}

void Smac_sendContent(MAC_crsPacket_t *pkt, uint8_t msduHandle)
{
    if (gIsInTheMiddle == true)
    {

    }
    gMsduHandle = msduHandle;
    memset(&gSmAckContentInfo, 0, sizeof(Smac_smAckContent_t));
    memcpy(gSmAckContentInfo.nodeMac, pkt->dstAddr, APIMAC_SADDR_EXT_LEN);

    uint8_t pBuf[1000] = { 0 };
    TX_buildBufFromSrct(pkt, pBuf);

//    Node_pendingPckts_t pendingPacket = { 0 };
//    memcpy(pendingPacket.content, pBuf, CRS_PKT_MAX_SIZE - 1);
//    Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);

    TX_sendPacket(pkt, smacFinishedSendingContentCb);
}

static void smacFinishedSendingContentCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT;
//        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_setTimeout(gSmAckContentInfo.nodeMac, timeoutCb, 300000);
        Node_startTimer(gSmAckContentInfo.nodeMac);

        RX_enterRx(smacReceivedContentAckCb, collectorPib.mac);
    }

    else
    {
        gSmacStateArray[0] = 6;

        smacEraseNode();
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}



static void timeoutCb(void *arg)
{
    gSmacStateArray[1] = 1;

    EasyLink_abort();

}


static void waitForSensorContentTimeoutCb(void *arg)
{
    //abort the rx
    //send the content again from pending with cb of

    Util_setEvent(&smacEvents, SMAC_RECIVE_CONTENT_TIMEOUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSem);

    EasyLink_abort();
}

static void smacReceivedContentAckCb(EasyLink_RxPacket *rxPacket,
                                     EasyLink_Status status)
{
    Node_stopTimer(gSmAckContentInfo.nodeMac);
    //we either got the ack or we lost the ack and got the content
    Node_nodeInfo_t node = { 0 };
    Node_getNode(gSmAckContentInfo.nodeMac, &node);
    //if ack arrived.
    if (status == EasyLink_Status_Success)
    {

        MAC_crsPacket_t pktReceived = { 0 };
        RX_buildStructPacket(&pktReceived, rxPacket->payload);
        if (pktReceived.commandId == MAC_COMMAND_ACK
                && pktReceived.seqSent == node.seqRcv
                && memcmp(node.mac, pktReceived.srcAddr, 8) == 0)
        {
//            gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT_ACK;
//            gSmacStateArrayIdx++;
            //TODO add to seq.
            Node_setSeqRcv(gSmAckContentInfo.nodeMac, node.seqRcv + 1);

            Node_setTimeout(gSmAckContentInfo.nodeMac, timeoutCb, 1000000);
            Node_startTimer(gSmAckContentInfo.nodeMac);
            //TODO set timer to content.
            //10us per tick so for 5ms we need 500 ticks
//            Node_setTimeout(gSmAckContentInfo.nodeMac,
//                            waitForSensorContentTimeoutCb, 100 * 20 * 1000);
//            Node_startTimer(gSmAckContentInfo.nodeMac);
            RX_enterRx(smacRecivedContentCb, collectorPib.mac);

            Util_setEvent(&smacEvents, SMAC_RECIVED_ACK_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
        }
        else
        {
            gSmacStateArray[0] = 3;

            smacEraseNode();

        }

    }

    //if timeout then retransmit
    else
    {
        gSmacStateArray[0] = 4;

        smacEraseNode();

//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;

    }
}

static void smacRecivedContentCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status)
{
    Node_stopTimer(gSmAckContentInfo.nodeMac);

    if (status == EasyLink_Status_Success)
    {
        RX_buildStructPacket(&gSmAckContentInfo.pkt, rxPacket->payload);
        if ( memcmp(gSmAckContentInfo.nodeMac, gSmAckContentInfo.pkt.srcAddr, 8) != 0)
        {
            gSmacStateArray[0] = 2;

            smacEraseNode();
            return;
        }

//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT;
//        gSmacStateArrayIdx++;


//        memcpy(gSmAckContentInfo.contentRsp, rxPacket->payload, rxPacket->len);
//        gSmAckContentInfo.contentRspLen = rxPacket->len;
        gSmAckContentInfo.contentRspRssi = rxPacket->rssi;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);

        Node_setSeqRcv(gSmAckContentInfo.nodeMac, node.seqRcv + 1);

        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, node.mac, 8);

        memcpy(pktRetAck.srcAddr, collectorPib.mac, 8);

        pktRetAck.seqSent = node.seqSend;
        pktRetAck.seqRcv = node.seqRcv + 1;

        pktRetAck.isNeedAck = 0;

        pktRetAck.len = 0;

//        uint8_t pBuf[100] = { 0 };
//        TX_buildBufFromSrct(&pktRetAck, pBuf);
//        Node_pendingPckts_t pendingPacket = { 0 };
//        memcpy(pendingPacket.content, pBuf, 100);
//        Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);


        TX_sendPacket(&pktRetAck, smacFinishedSendingContentAckCb);

        Util_setEvent(&smacEvents, SMAC_RECIVED_CONTENT_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
    }
    else
    {
        gSmacStateArray[0] = 1;

        smacEraseNode();

//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
    }
}

static void smacFinishedSendingContentAckCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_CONTENT_ACK;
//        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
        //        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);

//        RX_enterRx(smacRecivedContentAgainCb, collectorPib.mac);

        Util_setEvent(&smacEvents, SMAC_FINISHED_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);

    }
    else
    {
        gSmacStateArray[0] = 0;
        smacEraseNode();

//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
    }

}

static void smacFinishedSendingContentAckAgainCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_CONTENT_ACK;
//        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
        //        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);

        RX_enterRx(smacRecivedContentAgainCb, collectorPib.mac);

    }
    else
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
    }

}

static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT_AGAIN;
//        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);

        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, node.mac, 8);

        memcpy(pktRetAck.srcAddr, collectorPib.mac, 8);

        pktRetAck.seqSent = node.seqSend;
        pktRetAck.seqRcv = node.seqRcv;

        pktRetAck.isNeedAck = 0;

        pktRetAck.len = 0;

        uint8_t pBuf[100] = { 0 };
        TX_buildBufFromSrct(&pktRetAck, pBuf);
        Node_pendingPckts_t pendingPacket = { 0 };
        memcpy(pendingPacket.content, pBuf, 100);
        Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);

        TX_sendPacket(&pktRetAck, smacFinishedSendingContentAckCb);
    }
    else
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
    }
}

static void smacNotifyFailCb();

static void smacNotifySuccessCb(EasyLink_Status status)
{

}

static void smacEraseNode()
{
    Util_setEvent(&smacEvents, SMAC_ERASE_NODE_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSem);
}

void Smac_printStateMachine()
{

    int i = 0;
    for (i = 0; i < gSmacStateArrayIdx; ++i)
    {
        switch (gSmacStateArray[i])
        {
        case SMAC_RX_IDLE:
            CP_CLI_cliPrintf("\r\nSMAC_RX_IDLE");
            break;
        case SMAC_SENT_CONTENT:
            CP_CLI_cliPrintf("\r\nSMAC_SENT_CONTENT");
            break;
        case SMAC_SENT_CONTENT_AGAIN:
            CP_CLI_cliPrintf("\r\nSMAC_SENT_CONTENT_AGAIN");
            break;
        case SMAC_RECIVED_CONTENT_ACK:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_ACK");

            break;
        case SMAC_RECIVED_CONTENT:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT");

            break;
        case SMAC_FINISHED_SENDING_CONTENT_ACK:
            CP_CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_CONTENT_ACK");

            break;

        case SMAC_RECIVED_CONTENT_AGAIN:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_AGAIN");
            break;
        case SMAC_ERROR:
            CP_CLI_cliPrintf("\r\nSMAC_ERROR");
            break;
        default:
            break;
        }
    }
}