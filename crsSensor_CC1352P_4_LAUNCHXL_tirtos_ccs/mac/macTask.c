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
static Clock_Handle gclkMacTaskTimer;
static Clock_Params gclkMacTaskTimerParams;
static Clock_Struct gclkMacTaskTimerStruct;

typedef enum
{
    SMAC_RX_IDLE,
    SMAC_SEND_CONTENT,
    SMAC_WAIT_CONTENT_ACK,
    SMAC_WAIT_CONTENT,
    SMAC_SEND_CONTENT_ACK,
    SMAC_NOTIFY_FAIL,
    SMAC_NOTIFY_SUCCESS,
} Mac_smStateCodes_t;

typedef void (*SmFunc)(void);

typedef struct StateMachineAckContent
{

} Mac_smAckContent_t;

Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

/*! Storage for Events flags */
uint16_t macEvents = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void macFnx(UArg arg0, UArg arg1);
static void processTxDone();
static void processRxDone();
static void stateMachineAckContent(Mac_smStateCodes_t argStateCode);
static void smacRxIdle();
static void smacSendContent();
static void smacWaitContentAck();
static void smacWaitContent();
static void smacSendContentAck();
static void smacNotifyFail();
static void smacNotifySuccess();

static void recviedCollectorContentCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);
static void recviedCollectorContentAgainCb(EasyLink_RxPacket *rxPacket,
                                           EasyLink_Status status);
static void finishedSendingAckCb(EasyLink_Status status);
static void finishedSendingSensorContentCb(EasyLink_Status status);
static void recievedAckOnContentCb(EasyLink_RxPacket *rxPacket,
                                   EasyLink_Status status);
static void timeoutOnContentAckCb(EasyLink_RxPacket *rxPacket,
                                  EasyLink_Status status);
static void finishedSendingSensorContentAgainCb(EasyLink_Status status);
static void processContentTimerCb();

SmFunc gSmFpArr[7] = { smacRxIdle, smacSendContent, smacWaitContentAck,
                       smacWaitContent, smacSendContentAck, smacNotifyFail,
                       smacNotifySuccess };

void Mac_init()
{

    Task_Params_init(&macTaskParams);
    macTaskParams.stackSize = MAC_TASK_STACK_SIZE;
    macTaskParams.priority = MAC_TASK_PRIORITY;
    macTaskParams.stack = &macTaskStack;
    macTaskParams.arg0 = (UInt) 1000000;

    Task_construct(&macTask, macFnx, &macTaskParams, NULL);

    //gclkMacTask
    Clock_Params_init(&gclkMacTaskTimerParams);
    gclkMacTaskTimerParams.period = 0;
    gclkMacTaskTimerParams.startFlag = FALSE;
    /*   Construct a one-shot Clock Instance*/
    Clock_construct(&gclkMacTaskTimerStruct, NULL, 110000 / Clock_tickPeriod,
                    &gclkMacTaskTimerParams);
    gclkMacTaskTimer = Clock_handle(&gclkMacTaskTimerStruct);
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
    CollectorLink_init(macSemHandle);
    CollectorLink_collectorLinkInfo_t collector;
    uint8_t mac[8] = { 0xcf, 0x26, 0xf4, 0x14, 0x4b, 0x12, 0x00, 0x00 };
    memcpy(collector.mac, mac, 8);
    CollectorLink_updateCollector(&collector);

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
            CLI_cliPrintf("\r\nreceived packet");
            processRxDone();
            Util_clearEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
        }
        if (macEvents & MAC_TASK_ACK_RECEIVED)
        {
            CLI_cliPrintf("\r\nreceived ack");
            processRxDone();
            Util_clearEvent(&macEvents, MAC_TASK_ACK_RECEIVED);
        }

        if (macEvents & MAC_TASK_NODE_TIMEOUT_EVT)
        {
            CLI_cliPrintf("\r\nTimeout test");
            Util_clearEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
        }

        if (macEvents & MAC_TASK_CONTENT_READY)
        {
            CLI_cliPrintf("\r\ncontent ready to send");
            MAC_crsPacket_t pkt = { 0 };
            int i;
            for (i = 0; i < 50; i++)
            {
                pkt.payload[i] = rand();
            }
            pkt.commandId = MAC_COMMAND_DATA;
            uint8_t tmp[8] = { 0xcf, 0x26, 0xf4, 0x14, 0x4b, 0x12, 0x00, 0x00 };
            TX_sendPacket(&pkt, tmp, finishedSendingSensorContentCb);
            Util_clearEvent(&macEvents, MAC_TASK_CONTENT_READY);
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

void Mac_cliReceivePacket()
{

}

static void processTxDone()
{

}

static void processRxDone()
{
    MAC_crsPacket_t pkt;
    RX_getPacket(&pkt);
}

//rxdonecb of waiting for content from the collector
static void recviedCollectorContentCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status)
{
//check EasyLink_Status for success
//Increment the seqRecevied num in the CollectorLink_collectorLinkInfo_t struct
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    collectorLink.seqRcv++;
//Prepare an ack pkt(cmd id)
    MAC_crsPacket_t pkt = { 0 };
    pkt.commandId = MAC_COMMAND_ACK;
//copy ack packet into Node_pendingPckts_t strcut in the field 'content'
    memcpy(collectorLink.pendingPacket.content, pkt.payload, MAX_BYTES_PAYLOAD); //check what to copy exactly
    CollectorLink_updateCollector(&collectorLink);
//send ack with cb of 'finishedSendingAckCb'
    TX_sendPacket(&pkt, pkt.dstAddr, finishedSendingAckCb);

}

//rxdonecb of finishedSendingAckCb
static void recviedCollectorContentAgainCb(EasyLink_RxPacket *rxPacket,
                                           EasyLink_Status status)
{
//check seqRecived num- if it is the smaller by 1 from the seqSent in the struct of CollectorLink_collectorLinkInfo_t then:
    //send ack from  Node_pendingPckts_t strcut in the field 'content'
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    if ((collectorLink.seqRcv - 1) < collectorLink.seqSend)
    {
        TX_sendPacket(&collectorLink.pendingPacket.content, collectorLink.mac,
                      finishedSendingAckCb);    //check which cb
    }

//else- ???(shouldnt happen)

}

//txdonecb of sending ack
static void finishedSendingAckCb(EasyLink_Status status)
{
//start timer that process content, with cb 'processContentTimerCb' that raises event 'MAC_TASK_CONTENT_READY'
    Clock_setFunc(gclkMacTaskTimer, processContentTimerCb, NULL);
    Clock_start(gclkMacTaskTimer);

//Increment the seqSent num in the CollectorLink_collectorLinkInfo_t struct
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    collectorLink.seqSend++;
    CollectorLink_updateCollector(&collectorLink);
//enterRx with cb of 'recviedCollectorContentAgainCb'
    RX_enterRx(collectorLink.mac, recviedCollectorContentAgainCb);
}

//txdonecb of sending content event
static void finishedSendingSensorContentCb(EasyLink_Status status)
{
//Increment the seqSent num in the CollectorLink_collectorLinkInfo_t struct
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    collectorLink.seqSend++;
    CollectorLink_updateCollector(&collectorLink);
//enterRx with cb of 'recievedAckOnContentCb'
    RX_enterRx(collectorLink.mac, recievedAckOnContentCb);
//start timer with cb 'timeoutOnContentAckCb' that will resend content(from the struct Node_pendingPckts_t)
    Clock_setFunc(gclkMacTaskTimer, timeoutOnContentAckCb, NULL);
    Clock_start(gclkMacTaskTimer);
}

//rxdonecb after the sensor sent content, and is waiting for on ack from the collector on the content the sensor sent and now it received an ack
static void recievedAckOnContentCb(EasyLink_RxPacket *rxPacket,
                                   EasyLink_Status status)
{
//Increment the seqRecevied num in the Node_nodeInfo_t struct
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    collectorLink.seqRcv++;
    CollectorLink_updateCollector(&collectorLink);

//disable timer that would have timeout if no ack was received
    Clock_stop(gclkMacTaskTimer);

}
//the sensor send content but didnt recieve ack-retry
static void timeoutOnContentAckCb(EasyLink_RxPacket *rxPacket,
                                  EasyLink_Status status)
{
//send again from the struct Node_pendingPckts_t with txdonecb of 'finishedSendingSensorContentAgainCb'
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    TX_sendPacket(&collectorLink.pendingPacket.content, collectorLink.mac,
                  finishedSendingSensorContentAgainCb);
//start timer on waiting for ack with cb 'timeoutOnContentAckCb'
    Clock_setFunc(gclkMacTaskTimer, timeoutOnContentAckCb, NULL);
    Clock_start(gclkMacTaskTimer);
}

//txdonecb of sending content event agin in event of ack timeout
static void finishedSendingSensorContentAgainCb(EasyLink_Status status)
{
//enterRx with cb of 'recievedAckOnContentCb'
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    RX_enterRx(collectorLink.mac, recievedAckOnContentCb);
//if retry<MAX_RETRY:
//start timer with cb 'timeoutOnContentAckCb' that will resend content(from the struct Node_pendingPckts_t)

}

static void processContentTimerCb()
{
    Util_setEvent(&macEvents, MAC_TASK_CONTENT_READY);
    Semaphore_post(macSemHandle);
}
