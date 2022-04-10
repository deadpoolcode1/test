/*
 * macTask.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>
#include <xdc/runtime/System.h>

#include "macTask.h"
#include "cp_cli.h"
#include "mac_util.h"
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define MAC_TASK_STACK_SIZE    1024
#define MAC_TASK_PRIORITY      2

/******************************************************************************
 Local variables
 *****************************************************************************/

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

typedef void (*SmFunc)(void);

typedef struct StateMachineAckContent
{
    Mac_smStateCodes_t currState;
    uint8_t nodeMac[8];
    uint8_t contentRsp[256];

} Mac_smAckContent_t;

Mac_smAckContent_t gSmAckContentInfo = { 0 };

uint8_t gMacSrcAddr[8] = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };

Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

static Mac_smStateCodes_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;

/*! Storage for Events flags */
uint16_t macEvents = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void macFnx(UArg arg0, UArg arg1);
static void processTxDone();
static void processRxDone();
static void stateMachineAckContent(Mac_smStateCodes_t argStateCode);

static void smacSendContentAck(EasyLink_Status status);
static void smacNotifyFail();

static void ackTimeoutCb(void *arg);
static void smacRecivedContentCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status);
static void smacReceivedContentAckCb(EasyLink_RxPacket *rxPacket,
                                     EasyLink_Status status);
static void smacFinishedSendingContentCb(EasyLink_Status status);
static void sendContent(uint8_t mac[8]);
static void smacNotifySuccessCb(EasyLink_Status status);

static void smacFinishedSendingContentAckCb(EasyLink_Status status);

static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status);
static void smacFinishedSendingContentAckAgainCb(EasyLink_Status status);


SmFunc gSmFpArr[7] = { 0 };

void Mac_init()
{

    Task_Params_init(&macTaskParams);
    macTaskParams.stackSize = MAC_TASK_STACK_SIZE;
    macTaskParams.priority = MAC_TASK_PRIORITY;
    macTaskParams.stack = &macTaskStack;
    macTaskParams.arg0 = (UInt) 1000000;

    Task_construct(&macTask, macFnx, &macTaskParams, NULL);

}

static void macFnx(UArg arg0, UArg arg1)
{
    Semaphore_Params semParam;

    Semaphore_Params_init(&semParam);
    semParam.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&macSem, 0, &semParam);
    macSemHandle = Semaphore_handle(&macSem);

    // Initialize the EasyLink parameters to their default values
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);

    TX_init(macSemHandle);
    RX_init(macSemHandle);
    /*
     * Initialize EasyLink with the settings found in ti_easylink_config.h
     * Modify EASYLINK_PARAM_CONFIG in ti_easylink_config.h to change the default
     * PHY
     */
    if (EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_abort("EasyLink_init failed");
    }
    CLI_startREAD();
    Node_init(macSemHandle);
    Node_nodeInfo_t node = { 0 };
    uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
    memcpy(node.mac, tmp, 8);
    Node_addNode(&node);

    while (1)
    {
        //MAC_TASK_TX_DONE_EVT
        if (macEvents & MAC_TASK_CLI_UPDATE_EVT)
        {
            CLI_processCliUpdate();
            Util_clearEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);
        }

        if (macEvents & MAC_TASK_TX_DONE_EVT)
        {
            processTxDone();
            Util_clearEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_RX_DONE_EVT)
        {
            processRxDone();
            Util_clearEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_NODE_TIMEOUT_EVT)
        {
            CLI_cliPrintf("\r\nTimeout test");
            Util_clearEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
        }

        if (macEvents == 0)
        {
            Semaphore_pend(macSemHandle, BIOS_WAIT_FOREVER);
        }

    }
}

static void stateMachineAckContent(Mac_smStateCodes_t argStateCode)
{
    SmFunc stateFn;
    Mac_smStateCodes_t stateCode = argStateCode;
    while (1)
    {
        stateFn = gSmFpArr[stateCode];

    }
}

void Mac_cliUpdate()
{

    Util_setEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSemHandle);

}

void Mac_cliSendContent(uint8_t mac[8])
{
    if (mac == NULL)
    {
        uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
        sendContent(tmp);
    }
    else
    {
        sendContent(mac);
    }
}

static void processTxDone()
{

}

static void processRxDone()
{
    MAC_crsPacket_t pkt;
    RX_getPacket(&pkt);
}

static void sendContent(uint8_t mac[8])
{
    memset(&gSmAckContentInfo, 0, sizeof(Mac_smAckContent_t));
    memcpy(gSmAckContentInfo.nodeMac, mac, 8);

    Node_nodeInfo_t node = { 0 };
    Node_getNode(gSmAckContentInfo.nodeMac, &node);

    MAC_crsPacket_t pkt = { 0 };
    pkt.commandId = MAC_COMMAND_DATA;

    memcpy(pkt.dstAddr, mac, 8);

    memcpy(pkt.srcAddr, gMacSrcAddr, 8);

    pkt.seqSent = node.seqSend;
    pkt.seqRcv = node.seqRcv;

    pkt.isNeedAck = 1;

    int i = 0;

    for (i = 0; i < 50; i++)
    {
        pkt.payload[i] = i;
    }
    pkt.len = 50;

    uint8_t pBuf[256] = { 0 };
    TX_buildBufFromSrct(&pkt, pBuf);

    Node_pendingPckts_t pendingPacket = { 0 };
    memcpy(pendingPacket.content, pBuf, 100);
    Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);

    TX_sendPacket(&pkt, smacFinishedSendingContentCb);
}
//typedef enum
//{
//  MAC_COMMAND_DATA,
//  MAC_COMMAND_ACK
//} MAC_commandId_t;
//typedef enum
//{
//    SMAC_RX_IDLE,
//    SMAC_SENT_CONTENT,
//    SMAC_RECIVED_CONTENT_ACK,
//    SMAC_RECIVED_CONTENT,
//    SMAC_FINISHED_DENDING_CONTENT_ACK,
//    SMAC_NOTIFY_FAIL,
//    SMAC_NOTIFY_SUCCESS,
//} Mac_smStateCodes_t;
//SMAC_ERROR
static void smacFinishedSendingContentCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT;
        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_startTimer(gSmAckContentInfo.nodeMac);
        RX_enterRx(smacReceivedContentAckCb, gMacSrcAddr);
    }

    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}

static void smacFinishedSendingContentAgainCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT_AGAIN;
        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend);
        //10us per tick so for 5ms we need 500 ticks
        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_startTimer(gSmAckContentInfo.nodeMac);

        RX_enterRx(smacReceivedContentAckCb, gMacSrcAddr);
    }

    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}


static void ackTimeoutCb(void *arg)
{
    //abort the rx
    //send the content again from pending with cb of
    Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);

        MAC_crsPacket_t pkt = { 0 };

        RX_buildStructPacket(&pkt, node.pendingPacket.content);
        TX_sendPacket(&pkt, smacFinishedSendingContentAgainCb);

    EasyLink_abort();
}
//typedef struct Frame
//{
//    //payload len
//    uint16_t len;
//    uint16_t seqSent;
//    uint16_t seqRcv;
//    uint8_t srcAddr[8];
//    uint8_t dstAddr[8];
//    MAC_commandId_t commandId;
//    uint8_t isNeedAck;
//    uint8_t payload[CRS_PAYLOAD_MAX_SIZE];
//} MAC_crsPacket_t;

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
                && pktReceived.seqSent == node.seqRcv)
        {
            gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT_ACK;
            gSmacStateArrayIdx++;
            //TODO add to seq.
            Node_setSeqRcv(gSmAckContentInfo.nodeMac, node.seqRcv + 1);
            RX_enterRx(smacRecivedContentCb, gMacSrcAddr);
        }
        //we missed the ack and got the content. so only send ack on content
        else if (pktReceived.commandId == MAC_COMMAND_DATA
                && pktReceived.seqSent == node.seqRcv + 1)
        {
            gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT;
            gSmacStateArrayIdx++;
            Node_setSeqRcv(gSmAckContentInfo.nodeMac, node.seqRcv + 2);

            MAC_crsPacket_t pktRetAck = { 0 };
            pktRetAck.commandId = MAC_COMMAND_ACK;

            memcpy(pktRetAck.dstAddr, node.mac, 8);

            memcpy(pktRetAck.srcAddr, gMacSrcAddr, 8);

            pktRetAck.seqSent = node.seqSend;
            pktRetAck.seqRcv = node.seqRcv + 2;

            pktRetAck.isNeedAck = 0;

            pktRetAck.len = 0;

            uint8_t pBuf[100] = { 0 };
            TX_buildBufFromSrct(&pktRetAck, pBuf);
            Node_pendingPckts_t pendingPacket = { 0 };
            memcpy(pendingPacket.content, pBuf, 100);
            Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);

            TX_sendPacket(&pktRetAck, smacNotifySuccessCb);
        }

    }

    //if timeout then retransmit
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;

    }
}

static void smacRecivedContentCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT;
        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);

        Node_setSeqRcv(gSmAckContentInfo.nodeMac, node.seqRcv + 1);

        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, node.mac, 8);

        memcpy(pktRetAck.srcAddr, gMacSrcAddr, 8);

        pktRetAck.seqSent = node.seqSend;
        pktRetAck.seqRcv = node.seqRcv + 1;

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
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

static void smacFinishedSendingContentAckCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_CONTENT_ACK;
        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
        //        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);

        RX_enterRx(smacRecivedContentAgainCb, gMacSrcAddr);

    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }

}

static void smacFinishedSendingContentAckAgainCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_CONTENT_ACK;
        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
        //        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);

        RX_enterRx(smacRecivedContentAgainCb, gMacSrcAddr);

    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }

}

static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                 EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_CONTENT_AGAIN;
        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmAckContentInfo.nodeMac, &node);


        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, node.mac, 8);

        memcpy(pktRetAck.srcAddr, gMacSrcAddr, 8);

        pktRetAck.seqSent = node.seqSend;
        pktRetAck.seqRcv = node.seqRcv ;

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
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

static void smacNotifyFailCb();

static void smacNotifySuccessCb(EasyLink_Status status)
{

}
//typedef enum
//{
//    SMAC_RX_IDLE,
//    SMAC_SENT_CONTENT,
//    SMAC_RECIVED_CONTENT_ACK,
//    SMAC_RECIVED_CONTENT,
//    SMAC_FINISHED_DENDING_CONTENT_ACK,
//    SMAC_NOTIFY_FAIL,
//    SMAC_NOTIFY_SUCCESS,
//} Mac_smStateCodes_t;
void MAC_printSensorStateMachine()
{

    int i = 0;
    for (i = 0; i < gSmacStateArrayIdx; ++i)
    {
        switch (gSmacStateArray[i])
        {
        case SMAC_RX_IDLE:
            CLI_cliPrintf("\r\nSMAC_RX_IDLE");
            break;
        case SMAC_SENT_CONTENT:
            CLI_cliPrintf("\r\nSMAC_SENT_CONTENT");
            break;
        case SMAC_SENT_CONTENT_AGAIN:
                    CLI_cliPrintf("\r\nSMAC_SENT_CONTENT_AGAIN");
                    break;
        case SMAC_RECIVED_CONTENT_ACK:
            CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_ACK");

            break;
        case SMAC_RECIVED_CONTENT:
            CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT");

            break;
        case SMAC_FINISHED_SENDING_CONTENT_ACK:
            CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_CONTENT_ACK");

            break;

        case SMAC_RECIVED_CONTENT_AGAIN:
            CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_AGAIN");
            break;
        case SMAC_ERROR:
                    CLI_cliPrintf("\r\nSMAC_ERROR");
                    break;
        default:
            break;
        }
    }
}
//SMAC_SENT_CONTENT_AGAIN
