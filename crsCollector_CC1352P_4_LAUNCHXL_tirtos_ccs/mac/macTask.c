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
#include "mediator.h"
#include "stateMachines/sm_content_ack.h"
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
    Node_nodeInfo_t node = { 0 };
    uint8_t tmp[8] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
    memcpy(node.mac, tmp, 8);
    Node_addNode(&node);

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
            processkIncomingAppMsgs();
        }

    }
}

static void initCollectorPib()
{
    collectorPib.panId = CRS_GLOBAL_PAN_ID;
    collectorPib.shortAddr = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;
    CCFGRead_IEEE_MAC(collectorPib.mac);

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

        memcpy(pkt.srcAddr, collectorPib.mac, 8);

        Node_nodeInfo_t node = { 0 };
        Node_getNode(msg.msg->dstAddr.addr.extAddr, &node);

        pkt.seqSent = node.seqSend;
        pkt.seqRcv = node.seqRcv;

        pkt.isNeedAck = 1;
        memcpy(pkt.payload, msg.msg->msdu.p, msg.msg->msdu.len);
        pkt.len = msg.msg->msdu.len;
        pkt.panId = collectorPib.panId;

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
