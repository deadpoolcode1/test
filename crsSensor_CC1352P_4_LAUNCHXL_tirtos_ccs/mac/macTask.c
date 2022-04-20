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
#include "collectorLink.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mediator.h"
#include "stateMachines/sm_content_ack.h"
#include "stateMachines/sm_assoc.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define EXTADDR_OFFSET 0x2F0

#define MAC_TASK_STACK_SIZE    10024
#define MAC_TASK_PRIORITY      2

typedef enum
{
    MAC_SM_DISCOVERY, MAC_SM_RX_IDLE, MAC_SM_BEACON, MAC_SM_CONTENT_ACK, MAC_SM_OFF
} Mac_smStateCodes_t;

/******************************************************************************
 Global variables
 *****************************************************************************/
/*! Storage for Events flags */
uint16_t macEvents = 0;
MAC_sensorInfo_t sensorPib = { 0 };


/******************************************************************************
 Local variables
 *****************************************************************************/

static Mac_smStateCodes_t gState = MAC_SM_OFF;


static Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

static Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;



/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void macFnx(UArg arg0, UArg arg1);
static void processTxDone();
static void processRxDone();
static void stateMachineAckContent(Mac_smStateCodes_t argStateCode);

static void processkIncomingAppMsgs();
static void sendContent(uint8_t mac[8]);

static void initSensorPib();
static void CCFGRead_IEEE_MAC(ApiMac_sAddrExt_t addr);


static void buildBeaconPktFromBuf(MAC_crsBeaconPacket_t * beaconPkt, uint8_t* beaconBuff);
static void finishedSendingAssocReqCb(EasyLink_Status status);
static void recivedAsocRspCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);

static void waitForBeaconCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);

static void finishedSendingAssocRspAckCb(EasyLink_Status status);

static void buildAssocRspPktFromBuf(MAC_crsAssocRspPacket_t * beaconPkt, uint8_t* beaconBuff);


/******************************************************************************
 Public Functions
 *****************************************************************************/


void Mac_init()
{

    Task_Params_init(&macTaskParams);
    macTaskParams.stackSize = MAC_TASK_STACK_SIZE;
    macTaskParams.priority = MAC_TASK_PRIORITY;
    macTaskParams.stack = &macTaskStack;
    macTaskParams.arg0 = (UInt) 1000000;

    Task_construct(&macTask, macFnx, &macTaskParams, NULL);

}

/*!
 * @brief       Reads the IEEE extended MAC address from the CCFG
 * @param       addr - Extended address pointer
 */
static void CCFGRead_IEEE_MAC(ApiMac_sAddrExt_t addr)
{
    EasyLink_getIeeeAddr(addr);
}

static void macFnx(UArg arg0, UArg arg1)
{
    Semaphore_Params semParam;

    Semaphore_Params_init(&semParam);
    semParam.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&macSem, 0, &semParam);
    macSemHandle = Semaphore_handle(&macSem);

    Mediator_setMacSem(macSemHandle);

    // Initialize the EasyLink parameters to their default values
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);

    TX_init(macSemHandle);
    RX_init(macSemHandle);
    Smac_init(macSemHandle);
    Smas_init(macSemHandle);
    /*
     * Initialize EasyLink with the settings found in ti_easylink_config.h
     * Modify EASYLINK_PARAM_CONFIG in ti_easylink_config.h to change the default
     * PHY
     */
    if (EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_abort("EasyLink_init failed");
    }

    initSensorPib();

    CP_CLI_startREAD();

    CollectorLink_init();
//    CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
//    uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
//    memcpy(collectorLink.mac, tmp, 8);
//    CollectorLink_updateCollector(&collectorLink);


    RX_enterRx( Smas_waitForBeaconCb, NULL);
    gState = MAC_SM_BEACON;

//           RX_enterRx(collectorLink.mac, Smac_recviedCollectorContentCb);
    while (1)
    {
        if (gState == MAC_SM_OFF)
        {

        }
        else if (gState == MAC_SM_RX_IDLE)
        {

        }
        else if (gState == MAC_SM_BEACON)
        {
            Smas_process();

        }
        else if (gState == MAC_SM_DISCOVERY)
        {

        }
        else if (gState == MAC_SM_CONTENT_ACK)
        {
            Smac_process();
        }

        //MAC_TASK_TX_DONE_EVT
        if (macEvents & MAC_TASK_CLI_UPDATE_EVT)
        {
            CP_CLI_processCliUpdate();
            Util_clearEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);
        }

        if (macEvents & MAC_TASK_TX_DONE_EVT)
        {
//            processTxDone();
//            sensorPib.shortAddr = rspPkt.dstShortAddr;
//
//            CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
//            CollectorLink_getCollector(&collectorLink);
            CP_CLI_cliPrintf("\r\nConnected to collector shortAddr: %x", sensorPib.shortAddr);
            Util_clearEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_RX_DONE_EVT)
        {
            processRxDone();
            CP_CLI_cliPrintf("\r\nFinished sending assoc req");

            Util_clearEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_NODE_TIMEOUT_EVT)
        {
            CP_CLI_cliPrintf("\r\nTimeout test");
            Util_clearEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
        }

        if (macEvents == 0)
        {
            Semaphore_pend(macSemHandle, BIOS_WAIT_FOREVER);
            processkIncomingAppMsgs();
        }

    }
}

//static void waitForBeaconCb(EasyLink_RxPacket *rxPacket,
//                                      EasyLink_Status status)
//{
//
//    if (status == EasyLink_Status_Success)
//    {
//        MAC_crsBeaconPacket_t beaconPkt = { 0 };
//        buildBeaconPktFromBuf(&beaconPkt, rxPacket->payload);
//
//        if (beaconPkt.commandId == MAC_COMMAND_BEACON &&  beaconPkt.panId == CRS_GLOBAL_PAN_ID)
//        {
//            CollectorLink_collectorLinkInfo_t collectorLink = {0};
//            collectorLink.isVacant = false;
//            memcpy(collectorLink.mac, beaconPkt.srcAddr, 8);
//            collectorLink.shortAddr = beaconPkt.srcAddrShort;
//            CollectorLink_updateCollector(&collectorLink);
//
//            MAC_crsAssocReqPacket_t pktRec = { 0 };
//            pktRec.commandId = MAC_COMMAND_ASSOC_REQ;
//            memcpy(pktRec.srcAddr, sensorPib.mac, 8);
//
//            TX_sendPacketBuf(((uint8_t*)(&pktRec)), sizeof(MAC_crsAssocReqPacket_t), beaconPkt.srcAddr, finishedSendingAssocReqCb);
//
//        }
//        else
//        {
//            RX_enterRx(waitForBeaconCb, NULL);
//        }
//
//    }
//    else
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
////        gSmacStateArrayIdx++;
//    }
//}
//
//
//
//
//static void finishedSendingAssocReqCb(EasyLink_Status status)
//{
//    //content sent so wait for ack
//    if (status == EasyLink_Status_Success)
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT_AGAIN;
////        gSmacStateArrayIdx++;
////        Node_nodeInfo_t node = { 0 };
////        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//////        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend);
////        //10us per tick so for 5ms we need 500 ticks
////        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
////        Node_startTimer(gSmAckContentInfo.nodeMac);
//
//        RX_enterRx(recivedAsocRspCb, sensorPib.mac);
//        Util_setEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
//
//                    /* Wake up the application thread when it waits for clock event */
//                    Semaphore_post(macSemHandle);
//
//    }
//
//    else
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
////        gSmacStateArrayIdx++;
//        //        gCbCcaFailed();
//
//    }
//}
//
//static void recivedAsocRspCb(EasyLink_RxPacket *rxPacket,
//                                      EasyLink_Status status)
//{
//    //content sent so wait for ack
//    if (status == EasyLink_Status_Success)
//    {
//        MAC_crsAssocRspPacket_t rspPkt = { 0 };
//        buildAssocRspPktFromBuf(&rspPkt, rxPacket->payload);
//
//        sensorPib.shortAddr = rspPkt.dstShortAddr;
//
//        CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
//        CollectorLink_getCollector(&collectorLink);
//
//        MAC_crsPacket_t pktRetAck = { 0 };
//        pktRetAck.commandId = MAC_COMMAND_ACK;
//
//        memcpy(pktRetAck.dstAddr, collectorLink.mac, 8);
//
//        memcpy(pktRetAck.srcAddr, sensorPib.mac, 8);
//
//        pktRetAck.seqSent = collectorLink.seqSend;
//        pktRetAck.seqRcv = collectorLink.seqRcv;
//
//        pktRetAck.isNeedAck = 0;
//
//        pktRetAck.len = 0;
//
//        TX_sendPacket(&pktRetAck, finishedSendingAssocRspAckCb);
//
//
////        RX_enterRx(Smac_recviedCollectorContentCb, sensorPib.mac);
//
//    }
//    else
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
////        gSmacStateArrayIdx++;
//        //        gCbCcaFailed();
//
//    }
//}
//
//static void finishedSendingAssocRspAckCb(EasyLink_Status status)
//{
//    //content sent so wait for ack
//    if (status == EasyLink_Status_Success)
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_SENT_CONTENT_AGAIN;
////        gSmacStateArrayIdx++;
////        Node_nodeInfo_t node = { 0 };
////        Node_getNode(gSmAckContentInfo.nodeMac, &node);
//////        Node_setSeqSend(gSmAckContentInfo.nodeMac, node.seqSend);
////        //10us per tick so for 5ms we need 500 ticks
////        Node_setTimeout(gSmAckContentInfo.nodeMac, ackTimeoutCb, 100 * 20);
////        Node_startTimer(gSmAckContentInfo.nodeMac);
//
//        RX_enterRx(Smac_recviedCollectorContentCb, sensorPib.mac);
//
//        Util_setEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
//
//            /* Wake up the application thread when it waits for clock event */
//            Semaphore_post(macSemHandle);
//    }
//
//    else
//    {
////        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
////        gSmacStateArrayIdx++;
//        //        gCbCcaFailed();
//
//    }
//}
//
//
//
//static void buildBeaconPktFromBuf(MAC_crsBeaconPacket_t * beaconPkt, uint8_t* beaconBuff)
//{
//    memcpy(beaconPkt, (uint8_t*) beaconBuff, sizeof(MAC_crsBeaconPacket_t));
//
////    beaconPkt = (MAC_crsBeaconPacket_t*)beaconBuff;
//}
//
//static void buildAssocRspPktFromBuf(MAC_crsAssocRspPacket_t * beaconPkt, uint8_t* beaconBuff)
//{
//    memcpy(beaconPkt, (uint8_t*) beaconBuff, sizeof(MAC_crsAssocRspPacket_t));
//
////    beaconPkt = (MAC_crsBeaconPacket_t*)beaconBuff;
//}

static void initSensorPib()
{
    sensorPib.panId = CRS_GLOBAL_PAN_ID;
//    sensorPib.shortAddr = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;
    CCFGRead_IEEE_MAC(sensorPib.mac);

}

static void processkIncomingAppMsgs()
{
    Mediator_msgObjSentToMac_t msg = { 0 };
    bool rsp = Mediator_getNextAppMsg(&msg);
    if (rsp == false)
    {

    }
    else
    {
        gState = MAC_SM_CONTENT_ACK;

        MAC_crsPacket_t pkt = { 0 };

        pkt.commandId = MAC_COMMAND_DATA;

        memcpy(pkt.dstAddr, msg.msg->dstAddr.addr.extAddr, 8);

        memcpy(pkt.srcAddr, sensorPib.mac, 8);

        CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
        CollectorLink_getCollector( &collectorLink);

        pkt.seqSent = collectorLink.seqSend;
        pkt.seqRcv = collectorLink.seqRcv;

        pkt.isNeedAck = 1;
        memcpy(pkt.payload, msg.msg->msdu.p, msg.msg->msdu.len);
        pkt.len = msg.msg->msdu.len;
        pkt.panId = sensorPib.panId;

        Smac_sendContent(&pkt, msg.msg->msduHandle);
    }

}

bool MAC_createDataCnf(macMcpsDataCnf_t *rsp, uint8_t msduHandle,
                              ApiMac_status_t status)
{
    rsp->hdr.event = MAC_MCPS_DATA_CNF;
    rsp->hdr.status = status;

    rsp->msduHandle = msduHandle;
    return true;
}

bool MAC_sendCnfToApp(macMcpsDataCnf_t *dataCnf)
{
    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMcpsDataCnf_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);
    return true;

}

bool MAC_createDataInd(macMcpsDataInd_t *rsp, MAC_crsPacket_t *pkt,
                              ApiMac_status_t status)
{
    rsp->hdr.event = MAC_MCPS_DATA_IND;
    rsp->hdr.status = status;

    rsp->msdu.len = pkt->len;
    uint8_t *tmp = malloc(pkt->len + 50);
    memset(tmp, 0, pkt->len + 50);
    memcpy(tmp, pkt->payload, pkt->len);
    rsp->msdu.p = tmp;

    memcpy(rsp->mac.dstAddr.addr.extAddr, pkt->dstAddr, 8);
    memcpy(rsp->mac.srcAddr.addr.extAddr, pkt->srcAddr, 8);

//    rsp->mac.rssi =

    return true;
}

bool MAC_sendDataIndToApp(macMcpsDataInd_t *dataCnf)
{
    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMcpsDataInd_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);

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
//    memset(&gSmAckContentInfo, 0, sizeof(Mac_smAckContent_t));
//    memcpy(gSmAckContentInfo.nodeMac, mac, 8);
//
//    Node_nodeInfo_t node = { 0 };
//    Node_getNode(gSmAckContentInfo.nodeMac, &node);
//
//    MAC_crsPacket_t pkt = { 0 };
//    pkt.commandId = MAC_COMMAND_DATA;
//
//    memcpy(pkt.dstAddr, mac, 8);
//
//    memcpy(pkt.srcAddr, gMacSrcAddr, 8);
//
//    pkt.seqSent = node.seqSend;
//    pkt.seqRcv = node.seqRcv;
//
//    pkt.isNeedAck = 1;
//
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
//    Node_setPendingPckts(gSmAckContentInfo.nodeMac, &pendingPacket);
//
//    TX_sendPacket(&pkt, smacFinishedSendingContentCb);
}

//SMAC_SENT_CONTENT_AGAIN
