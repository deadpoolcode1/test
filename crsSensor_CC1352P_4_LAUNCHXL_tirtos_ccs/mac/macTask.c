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
    SMAC_RECIEVE_CONTENT,
    SMAC_SENDING_CONTENT,
    SMAC_FINISHED_SENDING_ACK,
    SMAC_FINISHED_SENDING_CONTENT,
    SMAC_RECIEVED_ACK_ON_CONTENT,
    SMAC_ERROR
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

static uint8_t gMacSrcAddr[8] =
        { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb };
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
    Clock_construct(&gclkMacTaskTimerStruct, NULL, 1100000 / Clock_tickPeriod,
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
    uint8_t mac[8] = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };
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
            //perapre pkt with all of its headers
            //copy pkt into pendingPkts
            //
            CLI_cliPrintf("\r\ncontent ready to send");
            MAC_crsPacket_t pkt = { 0 };
            int i;
            for (i = 0; i < 50; i++)
            {
                pkt.payload[i] = rand();
            }
            CollectorLink_collectorLinkInfo_t collectorLink;
            CollectorLink_getCollector(&collectorLink);
            pkt.commandId = MAC_COMMAND_DATA;
            pkt.seqSent = collectorLink.seqSend;
            pkt.seqRcv = collectorLink.seqRcv;
            pkt.isNeedAck = 1;
            memcpy(pkt.dstAddr, collectorLink.mac, 8);
            memcpy(pkt.srcAddr, gMacSrcAddr, 8);
            pkt.len = 50;
            EasyLink_abort();
            TX_sendPacket(&pkt, collectorLink.mac, finishedSendingSensorContentCb);
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

void Mac_cliReceivePacket(uint8_t dstMac[8])
{
    RX_enterRx(dstMac, recviedCollectorContentCb);

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
    gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIEVE_CONTENT;
    gSmacStateArrayIdx++;
    if (status == EasyLink_Status_Success)
    {
//check EasyLink_Status for success
//Increment the seqRecevied num in the CollectorLink_collectorLinkInfo_t struct
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqRcv++;
//Prepare an ack pkt(cmd id)
        MAC_crsPacket_t pkt = { 0 };
        pkt.commandId = MAC_COMMAND_ACK;
        pkt.seqSent = collectorLink.seqSend;
        pkt.seqRcv = collectorLink.seqRcv + 1;
        pkt.isNeedAck = 0;
        memcpy(pkt.dstAddr, collectorLink.mac, 8);
        memcpy(pkt.srcAddr, gMacSrcAddr, 8);
        pkt.len = 0;
//copy ack packet into Node_pendingPckts_t strcut in the field 'content'
        char pBuf[MAX_BYTES_PAYLOAD] = { 0 };
        TX_buildBufFromSrct(&pkt, pBuf);
        memcpy(collectorLink.pendingPacket.content, pBuf, MAX_BYTES_PAYLOAD); //check what to copy exactly
        CollectorLink_updateCollector(&collectorLink);
//send ack with cb of 'finishedSendingAckCb'
        TX_sendPacket(&pkt, collectorLink.mac, finishedSendingAckCb);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

//rxdonecb of finishedSendingAckCb
static void recviedCollectorContentAgainCb(EasyLink_RxPacket *rxPacket,
                                           EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {

//check seqRecived num- if it is the smaller by 1 from the seqSent in the struct of CollectorLink_collectorLinkInfo_t then:
        //send ack from  Node_pendingPckts_t strcut in the field 'content'
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        if ((collectorLink.seqRcv - 1) < collectorLink.seqSend) //TODO FIX IT WITH RXPACKET
        {
            TX_sendPacket(&collectorLink.pendingPacket.content,
                          collectorLink.mac, finishedSendingAckCb); //check which cb
        }

//else- ???(shouldnt happen)
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

//txdonecb of sending ack
static void finishedSendingAckCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_ACK;
        gSmacStateArrayIdx++;
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
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

//txdonecb of sending content event
static void finishedSendingSensorContentCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_FINISHED_SENDING_CONTENT;
        gSmacStateArrayIdx++;
//Increment the seqSent num in the CollectorLink_collectorLinkInfo_t struct
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqSend++;
        CollectorLink_updateCollector(&collectorLink);
//enterRx with cb of 'recievedAckOnContentCb'
        RX_enterRx(collectorLink.mac, recievedAckOnContentCb);
//start timer with cb 'timeoutOnContentAckCb' that will resend content(from the struct Node_pendingPckts_t)
//        Clock_setFunc(gclkMacTaskTimer, timeoutOnContentAckCb, NULL);
//        Clock_start(gclkMacTaskTimer);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }
}

//rxdonecb after the sensor sent content, and is waiting for on ack from the collector on the content the sensor sent and now it received an ack
static void recievedAckOnContentCb(EasyLink_RxPacket *rxPacket,
                                   EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIEVED_ACK_ON_CONTENT;
        gSmacStateArrayIdx++;
//Increment the seqRecevied num in the Node_nodeInfo_t struct
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqRcv++;
        CollectorLink_updateCollector(&collectorLink);

//disable timer that would have timeout if no ack was received
        Clock_stop(gclkMacTaskTimer);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }

}
//the sensor send content but didnt recieve ack-retry
static void timeoutOnContentAckCb(EasyLink_RxPacket *rxPacket,
                                  EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
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
}

//txdonecb of sending content event agin in event of ack timeout
static void finishedSendingSensorContentAgainCb(EasyLink_Status status)
{

    gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
    gSmacStateArrayIdx++;
    if (status == EasyLink_Status_Success)
    {
//enterRx with cb of 'recievedAckOnContentCb'
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        RX_enterRx(collectorLink.mac, recievedAckOnContentCb);
//if retry<MAX_RETRY:
//start timer with cb 'timeoutOnContentAckCb' that will resend content(from the struct Node_pendingPckts_t)
        Clock_setFunc(gclkMacTaskTimer, timeoutOnContentAckCb, NULL);
        Clock_start(gclkMacTaskTimer);
    }
}

static void processContentTimerCb()
{
    Util_setEvent(&macEvents, MAC_TASK_CONTENT_READY);
    Semaphore_post(macSemHandle);
}

void printSensorStateMachine()
{

    int i = 0;
    for (i = 0; i < gSmacStateArrayIdx; ++i)
    {
        switch (gSmacStateArray[i])
        {
        case SMAC_RX_IDLE:
            CLI_cliPrintf("\r\nSMAC_RX_IDLE");
            break;
        case SMAC_RECIEVE_CONTENT:
            CLI_cliPrintf("\r\nSMAC_RECIEVE_CONTENT");
            break;
        case SMAC_SENDING_CONTENT:
            CLI_cliPrintf("\r\nSMAC_SENDING_CONTENT");

            break;
        case SMAC_FINISHED_SENDING_ACK:
            CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_ACK");

            break;
        case SMAC_FINISHED_SENDING_CONTENT:
            CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_CONTENT");

            break;
        case SMAC_RECIEVED_ACK_ON_CONTENT:
            CLI_cliPrintf("\r\nSMAC_RECIEVED_ACK_ON_CONTENT");

            break;
        case SMAC_ERROR:
            CLI_cliPrintf("\r\nSMAC_ERROR");
            break;
        default:
            break;
        }
    }
}
