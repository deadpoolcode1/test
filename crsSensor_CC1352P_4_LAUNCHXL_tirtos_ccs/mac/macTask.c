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

#define MAC_TASK_STACK_SIZE    6000
#define MAC_TASK_PRIORITY      2

#define RSSI_ARR_SIZE 10

typedef enum
{
    MAC_SM_DISCOVERY, MAC_SM_RX_IDLE, MAC_SM_BEACON, MAC_SM_CONTENT_ACK, MAC_SM_OFF
} Mac_smStateCodes_t;

typedef struct _Sensor_rssi_t
{
    int8_t rssiArr[RSSI_ARR_SIZE];
    uint32_t rssiArrIdx;
    int8_t rssiMax;
    int8_t rssiMin;
    int8_t rssiAvg;
    int8_t rssiLast;

} Sensor_rssi_t;

/******************************************************************************
 Global variables
 *****************************************************************************/
/*! Storage for Events flags */
uint16_t macEvents = 0;
MAC_sensorInfo_t sensorPib = { 0 };
static uint8_t gIsLocked = 0;


/******************************************************************************
 Local variables
 *****************************************************************************/

static Mac_smStateCodes_t gState = MAC_SM_OFF;

static Sensor_rssi_t gRssiStrct = { 0 };


static Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

static Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;
static uint32_t gDiscoveryTime;


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



static void finishedSendingDiscoveryAckCb(EasyLink_Status status);
static void discoveryCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);

static void updateRssiStrct(int8_t rssi);
static void discoveryTimeoutCb(xdc_UArg arg);

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

    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;

    Clock_construct(&gClkStruct, NULL, 11000 / Clock_tickPeriod, &gClkParams);

    gClkHandle = Clock_handle(&gClkStruct);



//    CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
//    uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
//    memcpy(collectorLink.mac, tmp, 8);
//    CollectorLink_updateCollector(&collectorLink);

    MAC_moveToBeaconState();

//           RX_enterRx(collectorLink.mac, Smac_recviedCollectorContentCb);
    while (1)
    {
        if (gState == MAC_SM_OFF)
        {

        }
        else if (gState == MAC_SM_RX_IDLE)
        {


            if (macEvents & MAC_DISCOVERY_EVT)
            {
                CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
                CollectorLink_getCollector(&collectorLink);

                macMlmeDiscoveryInd_t rsp2 = { 0 };
                MAC_createDiscovryInd(&rsp2,
                                      collectorLink.mac);
                MAC_sendDiscoveryIndToApp(&rsp2);
                Util_clearEvent(&macEvents, MAC_DISCOVERY_EVT);
            }



            if (macEvents & MAC_ENTER_BEACON_STATE_EVT)
            {
                CP_CLI_cliPrintf("\r\nMAC_ENTER_BEACON_STATE_EVT");
                CollectorLink_collectorLinkInfo_t collectorLink = { 0 };
                                CollectorLink_getCollector(&collectorLink);
                macMlmeDisassociateInd_t rsp2 = { 0 };
                MAC_createDisssocInd(&rsp2, collectorLink.mac);
                MAC_sendDisassocIndToApp(&rsp2);
                CollectorLink_eraseCollector();
                MAC_moveToBeaconState();
                Util_clearEvent(&macEvents, MAC_ENTER_BEACON_STATE_EVT);
            }
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

void MAC_moveToSmriState()
{
    gState = MAC_SM_RX_IDLE;

    RX_enterRx(Smri_recivedPcktCb, sensorPib.mac);
}

void MAC_moveToSmacState()
{
    gState = MAC_SM_CONTENT_ACK;

    RX_enterRx(Smac_recviedCollectorContentCb, sensorPib.mac);

//    Semaphore_post(macSemHandle);

}

void MAC_moveToBeaconState()
{
    uint8_t dstAddr[8] = {CRS_GLOBAL_PAN_ID, 0, 0, 0, 0, 0, 0, 0};

       RX_enterRx( Smas_waitForBeaconCb, dstAddr);
       gState = MAC_SM_BEACON;

//    Semaphore_post(macSemHandle);

}



void Smri_recivedPcktCb(EasyLink_RxPacket *rxPacket,
                                  EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        if (((MAC_commandId_t) rxPacket->payload[0])
                == MAC_COMMAND_DISCOVERY )
        {
            gState = MAC_SM_DISCOVERY;
            discoveryCb(rxPacket, status);
//            Smac_recivedAssocReqCb(rxPacket, status);
        }
        else if(((MAC_commandId_t) rxPacket->payload[0])
                == MAC_COMMAND_DATA )
        {
            gState = MAC_SM_CONTENT_ACK;
            Smac_recviedCollectorContentCb(rxPacket, status);

        }

    }
    else
    {

    }
}


static void discoveryCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        MAC_stopDiscoveryClock();


        updateRssiStrct(rxPacket->rssi);

        MAC_crsPacket_t pktRec = { 0 };
//        uint8_t buff[1200] = {0};
        RX_buildStructPacket(&pktRec, rxPacket->payload);

        gIsLocked = pktRec.payload[0];

        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqRcv++;
        MAC_crsPacket_t pkt = { 0 };
        pkt.commandId = MAC_COMMAND_ACK;
        pkt.seqSent = collectorLink.seqSend;
        pkt.seqRcv = collectorLink.seqRcv;
        pkt.isNeedAck = 0;
        memcpy(pkt.dstAddr, collectorLink.mac, 8);
        memcpy(pkt.srcAddr, sensorPib.mac, 8);
        pkt.len = 4;


        pkt.payload[0] = (uint8_t)gRssiStrct.rssiAvg;
        pkt.payload[1] = (uint8_t)gRssiStrct.rssiLast;
        pkt.payload[2] = (uint8_t)gRssiStrct.rssiMax;
        pkt.payload[3] = (uint8_t)gRssiStrct.rssiMin;

        CollectorLink_updateCollector(&collectorLink);


        //send ack with cb of 'finishedSendingAckCb'
        TX_sendPacket(&pkt, finishedSendingDiscoveryAckCb);

        Util_setEvent(&macEvents, MAC_DISCOVERY_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSemHandle);

    }
    else
    {

    }
}

static void finishedSendingDiscoveryAckCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqSend++;
        CollectorLink_updateCollector(&collectorLink);

        MAC_startDiscoveryClock();

        MAC_moveToSmriState();

        //enterRx with cb of 'recviedCollectorContentAgainCb'
//        gIsDoneSendingDiscoveryAck = true;

    }
    else
    {
        CollectorLink_eraseCollector();
        MAC_moveToBeaconState();

    }
}

void MAC_updateDiscoveryTime(uint32_t discoveryTime)
{
    gDiscoveryTime = discoveryTime + 1;
}

void MAC_startDiscoveryClock()
{
    Clock_stop(gClkHandle);

    Clock_setFunc(gClkHandle, discoveryTimeoutCb, 0);
    Clock_setTimeout(gClkHandle, gDiscoveryTime * 100000);
    Clock_start(gClkHandle);

}

void MAC_stopDiscoveryClock()
{
    Clock_stop(gClkHandle);
}

static void discoveryTimeoutCb(xdc_UArg arg)
{
    if (gState == MAC_SM_RX_IDLE)
       {
           EasyLink_abort();
           Util_setEvent(&macEvents, MAC_ENTER_BEACON_STATE_EVT);

           /* Wake up the application thread when it waits for clock event */
           Semaphore_post(macSemHandle);
       }
       else
       {
           Clock_setFunc(gClkHandle, discoveryTimeoutCb, 0);
           Clock_setTimeout(gClkHandle, gDiscoveryTime * 100000);
           Clock_start(gClkHandle);
       }


}

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

        free(msg.msg->msdu.p);
        free(msg.msg);
    }

}



bool MAC_createDiscovryInd(macMlmeDiscoveryInd_t *rsp, sAddrExt_t deviceAddress)
{
    rsp->hdr.event = MAC_MLME_DISCOVERY_IND;
    memcpy(rsp->deviceAddress, deviceAddress, 8);
    rsp->isLocked = gIsLocked;
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
    rsp->givenShortAddr = sensorPib.shortAddr;

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

    rsp->mac.srcShortAddr = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;
    rsp->mac.dstShortAddr = sensorPib.shortAddr;

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


static void updateRssiStrct(int8_t rssi)
{
//    CRS_LOG(CRS_DEBUG, "START");
    gRssiStrct.rssiArr[gRssiStrct.rssiArrIdx] = rssi;

    if (gRssiStrct.rssiArrIdx + 1 < RSSI_ARR_SIZE)
    {
        gRssiStrct.rssiArrIdx += 1;
    }
    else if (gRssiStrct.rssiArrIdx + 1 == RSSI_ARR_SIZE)
    {
        gRssiStrct.rssiArrIdx = 0;
    }

    int i = 0;
    int16_t sum = 0;
    uint8_t numZeros = 0;
    for (i = 0; i < RSSI_ARR_SIZE; i++)
    {
        sum += gRssiStrct.rssiArr[i];
        if (gRssiStrct.rssiArr[i] == 0)
        {
            numZeros++;
        }
    }

    gRssiStrct.rssiAvg = sum / (RSSI_ARR_SIZE - numZeros);

    if (gRssiStrct.rssiMax == 0)
    {
        gRssiStrct.rssiMax = rssi;
    }
    if (gRssiStrct.rssiMin == 0)
    {
        gRssiStrct.rssiMin = rssi;
    }
    gRssiStrct.rssiLast = rssi;

    int x = 0;

    for (x = 0; x < RSSI_ARR_SIZE; x++)
    {
        if (gRssiStrct.rssiArr[x] != 0
                && gRssiStrct.rssiMax < gRssiStrct.rssiArr[x])
        {
            gRssiStrct.rssiMax = gRssiStrct.rssiArr[x];
        }

        if (gRssiStrct.rssiArr[x] != 0
                && gRssiStrct.rssiMin > gRssiStrct.rssiArr[x])
        {
            gRssiStrct.rssiMin = gRssiStrct.rssiArr[x];
        }
    }
}
