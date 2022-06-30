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
#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/PIN.h>

#include "macTask.h"
#include "cp_cli.h"
#include "mac_util.h"
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mediator.h"
#include "stateMachines/sm_content_ack.h"
#include "stateMachines/sm_assoc.h"
#include "stateMachines/sm_discovery.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define EXTADDR_OFFSET 0x2F0

#define MAC_TASK_STACK_SIZE    6000
#define MAC_TASK_PRIORITY      2

typedef enum
{
    MAC_SM_DISCOVERY,
    MAC_SM_RX_IDLE,
    MAC_SM_BEACON,
    MAC_SM_CONTENT_ACK,
    MAC_SM_OFF
} Mac_smStateCodes_t;

/******************************************************************************
 Global variables
 *****************************************************************************/
/*! Storage for Events flags */
uint16_t macEvents = 0;
MAC_collectorInfo_t collectorPib = { 0 };

/******************************************************************************
 Local variables
 *****************************************************************************/

static Mac_smStateCodes_t gState = MAC_SM_OFF;

static Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

static Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;

static bool gIsNeedToSendBeacon = false;

static bool gIsSendingBeacon = false;

static bool gIsNeedToSendDiscovery = false;

static bool gIsSendingDiscovery = false;


static uint16_t gDiscoveryNodeIdx = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void macFnx(UArg arg0, UArg arg1);
static void processTxDone();
static void processRxDone();
static void stateMachineAckContent(Mac_smStateCodes_t argStateCode);

static void processkIncomingAppMsgs();
static void sendContent(uint8_t mac[8]);

static void initCollectorPib();
static void CCFGRead_IEEE_MAC(ApiMac_sAddrExt_t addr);
static void beaconClockCb(xdc_UArg arg);
static void smasFinishedSendingBeaconCb(EasyLink_Status status);
static void buildBeaconBufFromPkt(MAC_crsBeaconPacket_t *beaconPkt,
                                  uint8_t *beaconBuff);
static void sendBeacon();

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

    /*
     * Initialize EasyLink with the settings found in ti_easylink_config.h
     * Modify EASYLINK_PARAM_CONFIG in ti_easylink_config.h to change the default
     * PHY
     */
    if (EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_abort("EasyLink_init failed");
    }

    initCollectorPib();

    CP_CLI_startREAD();

    Node_init(macSemHandle);
//    Smri_init(macSemHandle);
    Smas_init(macSemHandle);
    Smac_init(macSemHandle);
    Smd_init(macSemHandle);
    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;

    Clock_construct(&gClkStruct, NULL, 11000 / Clock_tickPeriod, &gClkParams);

    gClkHandle = Clock_handle(&gClkStruct);

    Clock_setFunc(gClkHandle, beaconClockCb, 0);
    Clock_setTimeout(gClkHandle, 1000);
//    Clock_start(gClkHandle);

//    Node_nodeInfo_t node = { 0 };
//    uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
//    memcpy(node.mac, tmp, 8);
//    Node_addNode(&node);

    RX_enterRx(Smri_recivedPcktCb, collectorPib.mac);

    gState = MAC_SM_RX_IDLE;
    gIsNeedToSendBeacon = true;

    while (1)
    {
        if (gState == MAC_SM_OFF)
        {

        }
        else if (gState == MAC_SM_RX_IDLE)
        {
            if (gIsSendingBeacon == false)
            {
                //            Smri_process();
                if (gIsNeedToSendBeacon == true
                        && ((macEvents & MAC_TASK_CLI_UPDATE_EVT) == 0))
                {
                    gIsNeedToSendBeacon = false;
                    CP_CLI_cliPrintf("\r\nSending beacon");

                    EasyLink_abort();
                    sendBeacon();
                }
                else if (gIsNeedToSendDiscovery == true
                        && ((macEvents & MAC_TASK_CLI_UPDATE_EVT) == 0))
                {
//                    PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,
//                                           !PIN_getOutputValue(CONFIG_PIN_RLED));
                    gState = MAC_SM_DISCOVERY;
                    EasyLink_abort();
                    gIsNeedToSendDiscovery = false;
                    Smd_startDiscovery();
                }
                else
                {
                    processkIncomingAppMsgs();
                }
            }

        }
        else if (gState == MAC_SM_BEACON)
        {
            Smas_process();
        }
        else if (gState == MAC_SM_DISCOVERY)
        {
            Smd_process();
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
            CP_CLI_cliPrintf("\r\nTimeout test");
            Util_clearEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
        }

        if (macEvents == 0)
        {
            Semaphore_pend(macSemHandle, BIOS_WAIT_FOREVER);
        }

    }
}

static void sendBeacon()
{
    gIsSendingBeacon = true;
    MAC_crsBeaconPacket_t beaconPkt = { 0 };
    beaconPkt.commandId = MAC_COMMAND_BEACON;
    beaconPkt.panId = collectorPib.panId;
    memcpy(beaconPkt.srcAddr, collectorPib.mac, 8);
    beaconPkt.srcAddrShort = collectorPib.shortAddr;

    beaconPkt.discoveryTime = CRS_BEACON_INTERVAL;


    uint8_t pBuf[800] = { 0 };
    buildBeaconBufFromPkt(&beaconPkt, pBuf);
    uint8_t dstAddr[8] = { CRS_GLOBAL_PAN_ID, 0, 0, 0, 0, 0, 0, 0 };


    TX_sendPacketBuf(pBuf, sizeof(MAC_crsBeaconPacket_t), dstAddr,
                     smasFinishedSendingBeaconCb);
////pBuf[600] = 'a';
//TX_sendPacketBuf(pBuf, 700, dstAddr,
//                     smasFinishedSendingBeaconCb);
}

static void smasFinishedSendingBeaconCb(EasyLink_Status status)
{
    gIsSendingBeacon = false;
//    PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));

//content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        Clock_setFunc(gClkHandle, beaconClockCb, 0);
//        Clock_setTimeout(gClkHandle, CRS_BEACON_INTERVAL*100000);
//        Clock_start(gClkHandle);
//        Util_setEvent(&smasEvents, SMAS_DEBUG_EVT);
//        Semaphore_post(macSem);
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,
//                               !PIN_getOutputValue(CONFIG_PIN_RLED));
        Clock_setFunc(gClkHandle, beaconClockCb, 0);
        Clock_setTimeout(gClkHandle, CRS_BEACON_INTERVAL * 100000);
        Clock_start(gClkHandle);
        RX_enterRx(Smri_recivedPcktCb, collectorPib.mac);
        Semaphore_post(macSemHandle);

    }

    else
    {
        CP_CLI_cliPrintf("\r\nWTF");

        Clock_setFunc(gClkHandle, beaconClockCb, 0);
        Clock_setTimeout(gClkHandle, CRS_BEACON_INTERVAL * 100000);
        Clock_start(gClkHandle);
        RX_enterRx(Smri_recivedPcktCb, collectorPib.mac);
        Semaphore_post(macSemHandle);


    }
}

static void buildBeaconBufFromPkt(MAC_crsBeaconPacket_t *beaconPkt,
                                  uint8_t *beaconBuff)
{
    memcpy(beaconBuff, (uint8_t*) beaconPkt, sizeof(MAC_crsBeaconPacket_t));
}

static void beaconClockCb(xdc_UArg arg)
{
    gIsNeedToSendDiscovery = true;

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSemHandle);
}

void Smri_recivedPcktCb(EasyLink_RxPacket *rxPacket, EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        if (((MAC_commandId_t) rxPacket->payload[0]) == MAC_COMMAND_ASSOC_REQ)
        {
            gState = MAC_SM_BEACON;

            Smac_recivedAssocReqCb(rxPacket, status);
        }
        else
        {
            MAC_moveToRxIdleState();
        }

    }
    else
    {
//        MAC_moveToRxIdleState();
    }
}

void MAC_moveToBeaconState()
{

    gIsNeedToSendBeacon = true;
    MAC_moveToRxIdleState();

}

void MAC_moveToRxIdleState()
{
    gState = MAC_SM_RX_IDLE;

    RX_enterRx(Smri_recivedPcktCb, collectorPib.mac);

    Semaphore_post(macSemHandle);

}

static void initCollectorPib()
{
    collectorPib.panId = CRS_GLOBAL_PAN_ID;
    collectorPib.shortAddr = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;
    CCFGRead_IEEE_MAC(collectorPib.mac);

}
static uint16_t gTotalSmacPackts = 0;
static uint8_t gStartOfPacket[100] = {0};

static void processkIncomingAppMsgs()
{
    Mediator_msgObjSentToMac_t msg = { 0 };
    bool rsp = Mediator_getNextAppMsg(&msg);
    if (rsp == false)
    {

    }
    else
    {
        gTotalSmacPackts++;

        MAC_crsPacket_t pkt = { 0 };

        pkt.commandId = MAC_COMMAND_DATA;

        memcpy(pkt.dstAddr, msg.msg->dstAddr.addr.extAddr, 8);

        memcpy(pkt.srcAddr, collectorPib.mac, 8);

        Node_nodeInfo_t node = { 0 };
        node.isVacant = true;
        bool rspStatus = Node_getNode(msg.msg->dstAddr.addr.extAddr, &node);

        if (node.isVacant == true || rspStatus == false)
        {


            macMcpsDataCnf_t rsp1 = { 0 };
            MAC_createDataCnf(&rsp1, msg.msg->msduHandle, ApiMac_status_noAck);
            MAC_sendCnfToApp(&rsp1);

            macMlmeDisassociateInd_t rsp2 = { 0 };

            MAC_createDisssocInd(&rsp2, msg.msg->dstAddr.addr.extAddr);
            MAC_sendDisassocIndToApp(&rsp2);

            free(msg.msg->msdu.p);
            free(msg.msg);
            return;
        }

        pkt.seqSent = node.seqSend;
        pkt.seqRcv = node.seqRcv;

        pkt.isNeedAck = 1;
        memcpy(gStartOfPacket, msg.msg->msdu.p, msg.msg->msdu.len);

        memcpy(pkt.payload, msg.msg->msdu.p, msg.msg->msdu.len);
        pkt.len = msg.msg->msdu.len;
        pkt.panId = collectorPib.panId;
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

        gState = MAC_SM_CONTENT_ACK;
        EasyLink_abort();
        Smac_sendContent(&pkt, msg.msg->msduHandle);
        free(msg.msg->msdu.p);
        free(msg.msg);
    }

}


bool MAC_createDiscovryInd(macMlmeDiscoveryInd_t *rsp, int8_t rssi, int8_t rssiMaxRemote, int8_t rssiMinRemote,
                           int8_t rssiAvgRemote,  int8_t rssiLastRemote, sAddrExt_t deviceAddress)
{
    rsp->hdr.event = MAC_MLME_DISCOVERY_IND;

    rsp->rssiRemote.rssiAvg = rssiAvgRemote;
    rsp->rssiRemote.rssiMax = rssiMaxRemote;
    rsp->rssiRemote.rssiMin = rssiMinRemote;
    rsp->rssiRemote.rssiLast = rssiLastRemote;

    memcpy(rsp->deviceAddress, deviceAddress, 8);
    rsp->rssi = rssi;
    return true;
}

bool MAC_sendDiscoveryIndToApp(macMlmeDiscoveryInd_t *dataCnf)
{
    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMlmeDiscoveryInd_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);
    return true;

}

bool MAC_createDisssocInd(macMlmeDisassociateInd_t *rsp,
                          sAddrExt_t deviceAddress)
{
    rsp->hdr.event = MAC_MLME_DISASSOCIATE_IND;

    memcpy(rsp->deviceAddress, deviceAddress, 8);

    return true;
}

bool MAC_sendDisassocIndToApp(macMlmeDisassociateInd_t *dataCnf)
{
    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMlmeDisassociateInd_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);
    return true;

}

bool MAC_createAssocInd(macMlmeAssociateInd_t *rsp, sAddrExt_t deviceAddress,
                        uint16_t shortAddr, ApiMac_status_t status)
{
    rsp->hdr.event = MAC_MLME_ASSOCIATE_IND;
    rsp->hdr.status = status;

    rsp->shortAddr = shortAddr;
    memcpy(rsp->deviceAddress, deviceAddress, 8);

    return true;
}

bool MAC_sendAssocIndToApp(macMlmeAssociateInd_t *dataCnf)
{
    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMlmeAssociateInd_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);
    return true;

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

    memcpy(rsp->mac.dstDeviceAddressLong, pkt->dstAddr, 8);
    memcpy(rsp->mac.srcDeviceAddressLong, pkt->srcAddr, 8);
    uint16_t srcShortAddr;
    MAC_getDeviceShortAddr(rsp->mac.srcDeviceAddressLong, &srcShortAddr);

    rsp->mac.srcShortAddr = srcShortAddr;
    rsp->mac.dstShortAddr = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;


    return true;
}

bool MAC_sendDataIndToApp(macMcpsDataInd_t *dataCnf)
{
//    PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

    macCbackEvent_t *cbEvent = malloc(sizeof(macCbackEvent_t));
    memset(cbEvent, 0, sizeof(macCbackEvent_t));

    memcpy(cbEvent, dataCnf, sizeof(macMcpsDataInd_t));

    Mediator_msgObjSentToApp_t msg = { 0 };
    msg.msg = cbEvent;

    Mediator_sendMsgToApp(&msg);

}

bool MAC_getDeviceShortAddr(uint8_t * mac, uint16_t* shortAddr)
{
    Node_nodeInfo_t node = { 0 };
    bool rsp = Node_getNode(mac, &node);
    *shortAddr = node.shortAddr;
    return rsp;
}

bool MAC_getDeviceLongAddr(uint16_t shortAddr, uint8_t* mac)
{
    Node_nodeInfo_t node = { 0 };
    bool rsp = Node_getNodeByShortAddr(shortAddr, &node);
    memcpy(mac, node.mac, 8);
    return rsp;

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
