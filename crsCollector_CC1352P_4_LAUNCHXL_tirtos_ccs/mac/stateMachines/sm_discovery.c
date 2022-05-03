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
#include "mac/node.h"
#include "mac/crs_tx.h"
#include "mac/crs_rx.h"
#include "mac/mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMD_RECIVE_ACK_MAX_RETRY_EVT 0x0001
#define SMD_RECIVED_ACK_EVT 0x0002
#define SMD_FINISHED_EVT 0x0004

#define SMD_DISCOVER_NEXT_NODE_EVT 0x0008
#define SMD_ERASE_NODE_EVT 0x0010



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
    bool isSuccess;
    struct
    {
        int8_t rssiMax;
        int8_t rssiMin;
        int8_t rssiAvg;
        int8_t rssiLast;
    } rssiRemote;
} Smac_smDiscovery_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *macSem = NULL;

static uint16_t smdEvents = 0;

static Smac_smDiscovery_t gSmDiscoveryInfo = { 0 };


static uint32_t gIdxDiscovery = 0;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


static bool sendNextDiscovery();
static void smacFinishedSendingDiscoveryCb(EasyLink_Status status);
static void smacReceivedDiscoveryAckCb(EasyLink_RxPacket *rxPacket,
                                              EasyLink_Status status);
static void timeoutCb(void *arg);

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
//        CP_CLI_cliPrintf("\r\nfinished discovery seq with node #0x%x",
//                                 gIdxDiscovery);
        if (gSmDiscoveryInfo.isSuccess == true)
        {
            macMlmeDiscoveryInd_t rsp2 = { 0 };
            MAC_createDiscovryInd(&rsp2, gSmDiscoveryInfo.contentRspRssi,
                                  gSmDiscoveryInfo.rssiRemote.rssiMax,
                                  gSmDiscoveryInfo.rssiRemote.rssiMin,
                                  gSmDiscoveryInfo.rssiRemote.rssiAvg,
                                  gSmDiscoveryInfo.rssiRemote.rssiLast,

                                  gSmDiscoveryInfo.nodeMac);
            MAC_sendDiscoveryIndToApp(&rsp2);
        }


        bool rsp = sendNextDiscovery();
        if (rsp == false)
        {
            MAC_moveToBeaconState();
        }
        Util_clearEvent(&smdEvents, SMD_DISCOVER_NEXT_NODE_EVT);
    }

    if (smdEvents & SMD_FINISHED_EVT)
    {

        Util_clearEvent(&smdEvents, SMD_FINISHED_EVT);
    }

    if (smdEvents & SMD_ERASE_NODE_EVT)
    {
        Node_nodeInfo_t node = { 0 };

        Node_getNode(gSmDiscoveryInfo.nodeMac, &node);
        Node_eraseNode(&node);

        macMlmeDisassociateInd_t rsp2 = { 0 };

        MAC_createDisssocInd(&rsp2, gSmDiscoveryInfo.nodeMac);
        MAC_sendDisassocIndToApp(&rsp2);

        Util_setEvent(&smdEvents, SMD_DISCOVER_NEXT_NODE_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
        Util_clearEvent(&smdEvents, SMD_ERASE_NODE_EVT);
    }


}

void Smd_startDiscovery()
{
    memset(&gSmDiscoveryInfo, 0, sizeof(Smac_smDiscovery_t));
    gIdxDiscovery = 0;

    bool rsp = sendNextDiscovery();
    if (rsp == false)
    {
        MAC_moveToBeaconState();
    }


}

static bool sendNextDiscovery()
{

    Node_nodeInfo_t node = { 0 };


    while(gIdxDiscovery < NUM_NODES && (Node_getNodeByIdx(gIdxDiscovery, &node) == false || node.isVacant == true))
    {
        gIdxDiscovery++;
    }

    if (gIdxDiscovery >= NUM_NODES)
    {
        return false;
    }

    memset(&gSmDiscoveryInfo, 0, sizeof(Smac_smDiscovery_t));
    memcpy(gSmDiscoveryInfo.nodeMac, node.mac, 8);


    MAC_crsPacket_t pkt = { 0 };
    pkt.commandId = MAC_COMMAND_DISCOVERY;

    memcpy(pkt.dstAddr, node.mac, 8);

    memcpy(pkt.srcAddr, collectorPib.mac, 8);

    pkt.seqSent = node.seqSend;
    pkt.seqRcv = node.seqRcv;

    pkt.isNeedAck = 1;

//    int i = 0;
//
//    for (i = 0; i < 50; i++)
//    {
//        pkt.payload[i] = i;
//    }
//    pkt.len = 50;
//
//    uint8_t pBuf[256] = { 0 };
//    TX_buildBufFromSrct(&pkt, pBuf);
//
//    Node_pendingPckts_t pendingPacket = { 0 };
//    memcpy(pendingPacket.content, pBuf, 100);
//    Node_setPendingPckts(gSmDiscoveryInfo.nodeMac, &pendingPacket);
//    EasyLink_abort();

    gIdxDiscovery++;

    TX_sendPacket(&pkt, smacFinishedSendingDiscoveryCb);
    return true;


}



static void smacFinishedSendingDiscoveryCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_DISCOVERY_CONTENT;
//        gSmacStateArrayIdx++;

        Node_nodeInfo_t node = { 0 };
        Node_getNode(gSmDiscoveryInfo.nodeMac, &node);
        Node_setSeqSend(gSmDiscoveryInfo.nodeMac, node.seqSend + 1);
        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
        Node_setTimeout(gSmDiscoveryInfo.nodeMac, timeoutCb, 3000);
        Node_startTimer(gSmDiscoveryInfo.nodeMac);

        RX_enterRx(smacReceivedDiscoveryAckCb, collectorPib.mac);
    }

    else
    {

        Util_setEvent(&smdEvents, SMD_ERASE_NODE_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
    }
}



static void smacReceivedDiscoveryAckCb(EasyLink_RxPacket *rxPacket,
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
                && pktReceived.seqSent == node.seqRcv
                && memcmp(gSmDiscoveryInfo.nodeMac, pktReceived.srcAddr, 8) == 0)
        {
//            gSmacStateArray[gSmacStateArrayIdx] =
//                    SMAC_RECIVED_DISCOVERY_CONTENT_ACK;
//            gSmacStateArrayIdx++;
            //TODO add to seq.
            Node_setSeqRcv(gSmDiscoveryInfo.nodeMac, node.seqRcv + 1);
            gSmDiscoveryInfo.isSuccess = true;
            gSmDiscoveryInfo.contentRspRssi = rxPacket->rssi;

            gSmDiscoveryInfo.rssiRemote.rssiAvg = (int8_t)pktReceived.payload[0];
            gSmDiscoveryInfo.rssiRemote.rssiLast = (int8_t)pktReceived.payload[1];
            gSmDiscoveryInfo.rssiRemote.rssiMax = (int8_t)pktReceived.payload[2];
            gSmDiscoveryInfo.rssiRemote.rssiMin = (int8_t)pktReceived.payload[3];

            Util_setEvent(&smdEvents, SMD_DISCOVER_NEXT_NODE_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
//            RX_enterRx(smacRecivedDiscoveryContentCb, collectorPib.mac);
        }
        //we missed the ack and got the content. so only send ack on content
        else
        {
            Util_setEvent(&smdEvents, SMD_ERASE_NODE_EVT);

                        /* Wake up the application thread when it waits for clock event */
                        Semaphore_post(macSem);
        }

    }

    //if timeout then retransmit
    else
    {
        Util_setEvent(&smdEvents, SMD_ERASE_NODE_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);

    }
}

static void timeoutCb(void *arg)
{
    EasyLink_abort();

}


