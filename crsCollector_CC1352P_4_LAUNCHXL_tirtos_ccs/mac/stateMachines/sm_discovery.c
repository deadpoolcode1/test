/*
 * sm_discovery.c
 *
 *  Created on: 14 Apr 2022
 *      Author: epc_4
 */




/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "sm_discovery.h"
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMD_RECIVE_ACK_MAX_RETRY_EVT 0x0001
#define SMD_RECIVED_ACK_EVT 0x0002
#define SMD_FINISHED_EVT 0x0004

#define SMD_DISCOVER_NEXT_NODE_EVT 0x0008


typedef enum
{
    SMAC_RX_IDLE,
      SMAC_SENT_CONTENT,
      SMAC_SENT_CONTENT_AGAIN,

      SMAC_SENT_DISCOVERY_CONTENT,
      SMAC_SENT_DISCOVERY_CONTENT_AGAIN,

      SMAC_RECIVED_CONTENT_ACK,
      SMAC_RECIVED_CONTENT,
      SMAC_RECIVED_CONTENT_AGAIN,

      SMAC_RECIVED_DISCOVERY_CONTENT_ACK,
      SMAC_RECIVED_DISCOVERY_CONTENT,
      SMAC_RECIVED_DISCOVERY_CONTENT_AGAIN,

      SMAC_FINISHED_SENDING_CONTENT_ACK,
      SMAC_FINISHED_SENDING_DISCOVERY_CONTENT_ACK,

      SMAC_NOTIFY_FAIL,
      SMAC_NOTIFY_SUCCESS,
      SMAC_ERROR
} Mac_smStateCodes_t;

typedef struct StateMachineDiscovery
{
    uint8_t nodeMac[8];
    uint8_t contentRspRssi;

} Smac_smDiscovery_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *macSem = NULL;

static uint16_t smdEvents = 0;

static Smac_smDiscovery_t gSmDiscoveryInfo = { 0 };

static Mac_smStateCodes_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;
static uint32_t gIdxDiscovery = 0;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void ackTimeoutCb(void *arg);
static void smacRecivedDiscoveryContentCb(EasyLink_RxPacket *rxPacket,
                                          EasyLink_Status status);
static void smacReceivedDiscoveryContentAckCb(EasyLink_RxPacket *rxPacket,
                                              EasyLink_Status status);
static void smacFinishedSendingDiscoveryContentCb(EasyLink_Status status);
static void smacFinishedSendingContentAgainCb(EasyLink_Status status);
static void sendDiscoveryContent(uint8_t mac[8]);
static void smacNotifySuccessCb(EasyLink_Status status);
static void smacNotifyFailCb();
static void smacFinishedSendingDiscoveryContentAckCb(EasyLink_Status status);
static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);
static void smacFinishedSendingContentAckAgainCb(EasyLink_Status status);



/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smd_init(void *sem)
{
    macSem = sem;
}

void Smd_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smdEvents & SMD_RECIVE_ACK_MAX_RETRY_EVT)
    {

        Util_clearEvent(&smdEvents, SMD_RECIVE_ACK_MAX_RETRY_EVT);
    }

    if (smdEvents & SMD_RECIVED_ACK_EVT)
    {
        Util_clearEvent(&smdEvents, SMD_RECIVED_ACK_EVT);
    }

    if (smdEvents & SMD_DISCOVER_NEXT_NODE_EVT)
    {
        CP_CLI_cliPrintf("\r\nfinished discovery seq with node #0x%x",
                                 gIdxDiscovery);
                   gIdxDiscovery++;

        Util_clearEvent(&smdEvents, SMD_DISCOVER_NEXT_NODE_EVT);
    }

    if (smdEvents & SMD_FINISHED_EVT)
    {

        Util_clearEvent(&smdEvents, SMD_FINISHED_EVT);
    }
}

void Smd_startDiscovery()
{
    memset(&gSmDiscoveryInfo, 0, sizeof(Smac_smDiscovery_t));
    Node_nodeInfo_t nodes[NUM_NODES] = { 0 };
    Node_getNodes(nodes);

    if (!(nodes[gIdxDiscovery].isVacant))
    {
//        uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
        sendDiscoveryContent(nodes[gIdxDiscovery].mac);
    }

}


void Smd_printStateMachine()
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
          case SMAC_SENT_DISCOVERY_CONTENT:
              CP_CLI_cliPrintf("\r\nSMAC_SENT_DISCOVERY_CONTENT");
              break;
          case SMAC_SENT_CONTENT_AGAIN:
              CP_CLI_cliPrintf("\r\nSMAC_SENT_CONTENT_AGAIN");
              break;
          case SMAC_RECIVED_CONTENT_ACK:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_ACK");
              break;
          case SMAC_RECIVED_CONTENT:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT");

          case SMAC_RECIVED_DISCOVERY_CONTENT:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_DISCOVERY_CONTENT");

          case SMAC_RECIVED_DISCOVERY_CONTENT_ACK:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_DISCOVERY_CONTENT_ACK");
              break;

              break;
          case SMAC_FINISHED_SENDING_CONTENT_ACK:
              CP_CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_CONTENT_ACK");

              break;

          case SMAC_RECIVED_CONTENT_AGAIN:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_AGAIN");
              break;

          case SMAC_RECIVED_DISCOVERY_CONTENT_AGAIN:
              CP_CLI_cliPrintf("\r\nSMAC_RECIVED_DISCOVERY_CONTENT_AGAIN");
              break;
          case SMAC_ERROR:
              CP_CLI_cliPrintf("\r\nSMAC_ERROR");
              break;
          default:
              break;
        }
    }
}



static void sendDiscoveryContent(uint8_t mac[8])
{

    Node_nodeInfo_t nodes[NUM_NODES];

    memset(&gSmDiscoveryInfo, 0, sizeof(Smac_smDiscovery_t));
    memcpy(gSmDiscoveryInfo.nodeMac, mac, 8);

    Node_nodeInfo_t node = { 0 };
    Node_getNode(gSmDiscoveryInfo.nodeMac, &node);

    MAC_crsPacket_t pkt = { 0 };
    pkt.commandId = MAC_COMMAND_DISCOVERY;

    memcpy(pkt.dstAddr, mac, 8);

    memcpy(pkt.srcAddr, collectorPib.mac, 8);

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
    Node_setPendingPckts(gSmDiscoveryInfo.nodeMac, &pendingPacket);
    EasyLink_abort();
    TX_sendPacket(&pkt, smacFinishedSendingDiscoveryContentCb);
}



static void smacFinishedSendingDiscoveryContentCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_DISCOVERY_CONTENT;
        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmDiscoveryInfo.nodeMac, &node);
        Node_setSeqSend(gSmDiscoveryInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_setTimeout(gSmDiscoveryInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_startTimer(gSmDiscoveryInfo.nodeMac);
        RX_enterRx(smacReceivedDiscoveryContentAckCb, collectorPib.mac);
    }

    else
    {

        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}


static void ackTimeoutCb(void *arg){}

static void smacReceivedDiscoveryContentAckCb(EasyLink_RxPacket *rxPacket,
                                              EasyLink_Status status)
{
    Node_stopTimer(gSmDiscoveryInfo.nodeMac);
    //we either got the ack or we lost the ack and got the content
    Node_nodeInfo_t node = { 0 };
    Node_getNode(gSmDiscoveryInfo.nodeMac, &node);
    //if ack arrived.
    if (status == EasyLink_Status_Success)
    {

        MAC_crsPacket_t pktReceived = { 0 };
        RX_buildStructPacket(&pktReceived, rxPacket->payload);

        if (pktReceived.commandId == MAC_COMMAND_ACK
                && pktReceived.seqSent == node.seqRcv)
        {
            gSmacStateArray[gSmacStateArrayIdx] =
                    SMAC_RECIVED_DISCOVERY_CONTENT_ACK;
            gSmacStateArrayIdx++;
            //TODO add to seq.
            Node_setSeqRcv(gSmDiscoveryInfo.nodeMac, node.seqRcv + 1);
            RX_enterRx(smacRecivedDiscoveryContentCb, collectorPib.mac);
        }
        //we missed the ack and got the content. so only send ack on content
        else if (pktReceived.commandId == MAC_COMMAND_DATA
                && pktReceived.seqSent == node.seqRcv + 1)
        {
            gSmacStateArray[gSmacStateArrayIdx] =
                    SMAC_RECIVED_DISCOVERY_CONTENT;
            gSmacStateArrayIdx++;
            Node_setSeqRcv(gSmDiscoveryInfo.nodeMac, node.seqRcv + 2);

            MAC_crsPacket_t pktRetAck = { 0 };
            pktRetAck.commandId = MAC_COMMAND_ACK;

            memcpy(pktRetAck.dstAddr, node.mac, 8);

            memcpy(pktRetAck.srcAddr, collectorPib.mac, 8);

            pktRetAck.seqSent = node.seqSend;
            pktRetAck.seqRcv = node.seqRcv + 2;

            pktRetAck.isNeedAck = 0;

            pktRetAck.len = 0;

            uint8_t pBuf[100] = { 0 };
            TX_buildBufFromSrct(&pktRetAck, pBuf);
            Node_pendingPckts_t pendingPacket = { 0 };
            memcpy(pendingPacket.content, pBuf, 100);
            Node_setPendingPckts(gSmDiscoveryInfo.nodeMac, &pendingPacket);
            Util_setEvent(&macEvents, SMD_DISCOVER_NEXT_NODE_EVT);
            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
            TX_sendPacket(&pktRetAck, smacNotifySuccessCb);
        }

    }

    //if timeout then retransmit
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;

//        //send the content again from pending with cb of
//        Node_nodeInfo_t node = { 0 };
//            Node_getNode(gSmAckContentInfo.nodeMac, &node);
//
//            MAC_crsPacket_t pkt = { 0 };
//
//            RX_buildStructPacket(&pkt, node.pendingPacket.content);
//            TX_sendPacket(&pkt, smacFinishedSendingContentAgainCb);

    }
}





static void smacRecivedDiscoveryContentCb(EasyLink_RxPacket *rxPacket,
                                          EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIVED_DISCOVERY_CONTENT;
        gSmacStateArrayIdx++;
        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmDiscoveryInfo.nodeMac, &node);

        Node_setSeqRcv(gSmDiscoveryInfo.nodeMac, node.seqRcv + 1);

        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, node.mac, 8);

        memcpy(pktRetAck.srcAddr, collectorPib.mac, 8);

        pktRetAck.seqSent = node.seqSend;
        pktRetAck.seqRcv = node.seqRcv + 1;

        pktRetAck.isNeedAck = 0;

        pktRetAck.len = 0;

        uint8_t pBuf[100] = { 0 };
        TX_buildBufFromSrct(&pktRetAck, pBuf);
        Node_pendingPckts_t pendingPacket = { 0 };
        memcpy(pendingPacket.content, pBuf, 100);
        Node_setPendingPckts(gSmDiscoveryInfo.nodeMac, &pendingPacket);
        TX_sendPacket(&pktRetAck, smacFinishedSendingDiscoveryContentAckCb);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}


static void smacFinishedSendingDiscoveryContentAckCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] =
                SMAC_FINISHED_SENDING_DISCOVERY_CONTENT_ACK;
        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmDiscoveryInfo.nodeMac, &node);
        Node_setSeqSend(gSmDiscoveryInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
        //        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);

        RX_enterRx(smacRecivedContentAgainCb, collectorPib.mac);

        Util_setEvent(&macEvents, SMD_DISCOVER_NEXT_NODE_EVT);
        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }

}

static void smacRecivedContentAgainCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status){}

static void smacNotifyFailCb(){}
static void smacNotifySuccessCb(EasyLink_Status status)
{

}
