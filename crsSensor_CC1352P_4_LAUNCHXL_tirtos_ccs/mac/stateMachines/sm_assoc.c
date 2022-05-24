/*
 * sm_assoc.c
 *
 *  Created on: 20 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>

#include "sm_assoc.h"
#include "sm_content_ack.h"

#include "mac/collectorLink.h"
#include "mac/crs_tx.h"
#include "mac/crs_rx.h"
#include "mac/mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMAS_JOINED_NETWORK_EVT 0x0001
#define SMAS_SEND_ASSOC_REQ_EVT 0x0002

typedef enum
{
    SMAC_RX_IDLE,
//    SMAC_SENT_CONTENT,
//    SMAC_SENT_CONTENT_AGAIN,
//
//    SMAC_RECIVED_CONTENT_ACK,
//    SMAC_RECIVED_CONTENT,
//    SMAC_RECIVED_CONTENT_AGAIN,
//
//    SMAC_FINISHED_SENDING_CONTENT_ACK,
//    SMAC_NOTIFY_FAIL,
//    SMAC_NOTIFY_SUCCESS,
//    SMAC_ERROR
} Mac_smStateCodes_t;

typedef struct StateMachineAsoc
{
    uint8_t nodeMac[8];
    uint16_t shortAddr;
    MAC_crsPacket_t pkt;
    uint8_t contentRsp[256];
    uint16_t contentRspLen;
    uint8_t contentRspRssi;

} Smas_smAsoc_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *macSem = NULL;

static uint16_t smasEvents = 0;

static Smas_smAsoc_t gSmAsocInfo = { 0 };

static Mac_smStateCodes_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;

static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void sendAssocReq();
static void buildBeaconPktFromBuf(MAC_crsBeaconPacket_t * beaconPkt, uint8_t* beaconBuff);
static void finishedSendingAssocReqCb(EasyLink_Status status);
static void recivedAsocRspCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);

//static void waitForBeaconCb(EasyLink_RxPacket *rxPacket,
//                                      EasyLink_Status status);

static void finishedSendingAssocRspAckCb(EasyLink_Status status);

static void buildAssocRspPktFromBuf(MAC_crsAssocRspPacket_t * beaconPkt, uint8_t* beaconBuff);

static void assocReqTimeoutCb(xdc_UArg arg);
static void assocReqTimeout2Cb(xdc_UArg arg);

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smas_init(void *sem)
{
    macSem = sem;
//    Clock_Params_init(&gClkParams);
//    gClkParams.period = 0;
//    gClkParams.startFlag = FALSE;
//
//    Clock_construct(&gClkStruct, NULL, 11000 / Clock_tickPeriod,
//                    &gClkParams);
//
//    gClkHandle = Clock_handle(&gClkStruct);
//
//    Clock_setFunc(gClkHandle, beaconClockCb, 0);
//    Clock_setTimeout(gClkHandle, 10000);
//    Clock_start(gClkHandle);

}

void Smas_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smasEvents & SMAS_JOINED_NETWORK_EVT)
    {

        macMlmeAssociateInd_t rsp = { 0 };
        MAC_createAssocInd(&rsp, gSmAsocInfo.nodeMac, gSmAsocInfo.shortAddr,
                           ApiMac_status_success);
        MAC_startDiscoveryClock();

        MAC_moveToSmriState();
        MAC_sendAssocIndToApp(&rsp);

        CP_CLI_cliPrintf("\r\nConnected to collector shortAddr: %x", sensorPib.shortAddr);

        Util_clearEvent(&smasEvents, SMAS_JOINED_NETWORK_EVT);
    }

    if (smasEvents & SMAS_SEND_ASSOC_REQ_EVT)
    {
        CP_CLI_cliPrintf("\r\nFinished sending assoc req");

        EasyLink_abort();
        sendAssocReq();
        Util_clearEvent(&smasEvents, SMAS_SEND_ASSOC_REQ_EVT);
    }
}

static void sendAssocReq()
{
    MAC_crsAssocReqPacket_t pktRec = { 0 };
    pktRec.commandId = MAC_COMMAND_ASSOC_REQ;
    memcpy(pktRec.srcAddr, sensorPib.mac, 8);

    CollectorLink_collectorLinkInfo_t collectorLink = {0};
    CollectorLink_getCollector(&collectorLink);


    TX_sendPacketBuf(((uint8_t*) (&pktRec)), sizeof(MAC_crsAssocReqPacket_t),
                     collectorLink.mac, finishedSendingAssocReqCb);

}

void Smas_waitForBeaconCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status)
{

    if (status == EasyLink_Status_Success)
    {
        MAC_crsBeaconPacket_t beaconPkt = { 0 };
        buildBeaconPktFromBuf(&beaconPkt, rxPacket->payload);

        if (beaconPkt.commandId == MAC_COMMAND_BEACON &&  beaconPkt.panId == CRS_GLOBAL_PAN_ID)
        {
//            gDiscoveryTime = beaconPkt.discoveryTime;
            memcpy(gSmAsocInfo.nodeMac, beaconPkt.srcAddr, 8);
            gSmAsocInfo.shortAddr = beaconPkt.srcAddrShort;
            MAC_updateDiscoveryTime(beaconPkt.discoveryTime);
            CollectorLink_collectorLinkInfo_t collectorLink = {0};
            collectorLink.isVacant = false;
            memcpy(collectorLink.mac, beaconPkt.srcAddr, 8);
            collectorLink.shortAddr = beaconPkt.srcAddrShort;
            CollectorLink_updateCollector(&collectorLink);

//            CollectorLink_collectorLinkInfo_t collectorLink = {0};
                CollectorLink_getCollector(&collectorLink);

                CollectorLink_setTimeout(assocReqTimeoutCb, (Clock_getTicks() % 250)*2000);
                CollectorLink_startTimer();
//            Util_setEvent(&smasEvents, SMAS_SEND_ASSOC_REQ_EVT);
//
//            /* Wake up the application thread when it waits for clock event */
//            Semaphore_post(macSem);

        }
        else
        {
            uint8_t dstAddr[8] = {CRS_GLOBAL_PAN_ID, 0, 0, 0, 0, 0, 0, 0};

            RX_enterRx(Smas_waitForBeaconCb, dstAddr);
        }

    }
    else
    {
        CollectorLink_eraseCollector();

                MAC_moveToBeaconState();
    }
}




static void finishedSendingAssocReqCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT_AGAIN;
//        gSmacStateArrayIdx++;
//        Node_nodeInfo_t node = { 0 };
//        Node_getNode(gSmAckContentInfo.nodeMac, &node);
////        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend);
//        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
//        Node_startTimer(gSmAckContentInfo.nodeMac);

//        Util_setEvent(&smasEvents, SMAS_JOINED_NETWORK_EVT);
//
//                    /* Wake up the application thread when it waits for clock event */
//                    Semaphore_post(macSem);

        CollectorLink_collectorLinkInfo_t collectorLink = {0};
            CollectorLink_getCollector(&collectorLink);

            CollectorLink_setTimeout(assocReqTimeout2Cb, 100000);
            CollectorLink_startTimer();
            RX_enterRx(recivedAsocRspCb, sensorPib.mac);

//        Util_setEvent(&smasEvents, MAC_TASK_RX_DONE_EVT);
//
//                    /* Wake up the application thread when it waits for clock event */
//                    Semaphore_post(macSemHandle);

    }

    else
    {
        CollectorLink_eraseCollector();

        MAC_moveToBeaconState();
        CP_CLI_cliPrintf("\r\nWTF");
    }
}

static void recivedAsocRspCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status)
{
    CollectorLink_stopTimer();

    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {

        MAC_crsAssocRspPacket_t rspPkt = { 0 };
        buildAssocRspPktFromBuf(&rspPkt, rxPacket->payload);

        sensorPib.shortAddr = rspPkt.dstShortAddr;

        CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
        CollectorLink_getCollector(&collectorLink);

        MAC_crsPacket_t pktRetAck = { 0 };
        pktRetAck.commandId = MAC_COMMAND_ACK;

        memcpy(pktRetAck.dstAddr, collectorLink.mac, 8);

        memcpy(pktRetAck.srcAddr, sensorPib.mac, 8);

        pktRetAck.seqSent = collectorLink.seqSend;
        pktRetAck.seqRcv = collectorLink.seqRcv;

        pktRetAck.isNeedAck = 0;

        pktRetAck.len = 0;

        TX_sendPacket(&pktRetAck, finishedSendingAssocRspAckCb);


//        RX_enterRx(Smac_recviedCollectorContentCb, sensorPib.mac);

    }
    else
    {
        CollectorLink_eraseCollector();

                MAC_moveToBeaconState();

    }
}

static void finishedSendingAssocRspAckCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT_AGAIN;
//        gSmacStateArrayIdx++;
//        Node_nodeInfo_t node = { 0 };
//        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend);
//        //10us per tick so for 5ms we need 500 ticks
//        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
//        Node_startTimer(gSmAckContentInfo.nodeMac);
//        RX_enterRx(Smac_recviedCollectorContentCb, sensorPib.mac);


        Util_setEvent(&smasEvents, SMAS_JOINED_NETWORK_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
    }

    else
    {
        CollectorLink_eraseCollector();

                MAC_moveToBeaconState();

    }
}



static void buildBeaconPktFromBuf(MAC_crsBeaconPacket_t * beaconPkt, uint8_t* beaconBuff)
{
    memcpy(beaconPkt, (uint8_t*) beaconBuff, sizeof(MAC_crsBeaconPacket_t));

//    beaconPkt = (MAC_crsBeaconPacket_t*)beaconBuff;
}

static void buildAssocRspPktFromBuf(MAC_crsAssocRspPacket_t * beaconPkt, uint8_t* beaconBuff)
{
    memcpy(beaconPkt, (uint8_t*) beaconBuff, sizeof(MAC_crsAssocRspPacket_t));

//    beaconPkt = (MAC_crsBeaconPacket_t*)beaconBuff;
}

static void assocReqTimeoutCb(xdc_UArg arg)
{
    Util_setEvent(&smasEvents, SMAS_SEND_ASSOC_REQ_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSem);
}

static void assocReqTimeout2Cb(xdc_UArg arg)
{
   EasyLink_abort();
}


