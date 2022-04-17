/*
 * sm_content_ack.c
 *
 *  Created on: 13 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "sm_content_ack.h"
#include "collectorLink.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMAC_RECIVE_ACK_MAX_RETRY_EVT 0x0001
#define SMAC_RECIVE_CONTENT_TIMEOUT_EVT 0x0002
#define SMAC_RECIVED_ACK_EVT 0x0004
#define SMAC_RECIVED_CONTENT_EVT 0x0008
#define SMAC_FINISHED_EVT 0x0010

typedef enum
{
    SMAC_RX_IDLE,
    SMAC_RECIEVE_CONTENT,
    SMAC_SENDING_CONTENT,
    SMAC_FINISHED_SENDING_ACK,
    SMAC_FINISHED_SENDING_CONTENT,
    SMAC_RECIEVED_ACK_ON_CONTENT,
    SMAC_TIMEOUT_ON_CONTENT,
    SMAC_RECIEVED_CONTENT_AGAIN,
    SMAC_FINISHED_SENDING_RETRY_CONTENT,
    SMAC_ERROR
} Mac_smStateCodes_t;

typedef struct StateMachineAckContent
{
    uint8_t nodeMac[8];
    MAC_crsPacket_t pkt;
    uint8_t contentRsp[256];
    uint16_t contentRspLen;
    uint8_t contentRspRssi;

} Smac_smAckContent_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

void *macSem = NULL;

static uint16_t smacEvents = 0;

static volatile bool gIsDoneSendingAck = false;

static Smac_smAckContent_t gSmAckContentInfo = { 0 };

static Mac_smStateCodes_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;

static uint8_t gMsduHandle = 0;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/



static void recviedCollectorContentAgainCb(EasyLink_RxPacket *rxPacket,
                                           EasyLink_Status status);

static void finishedSendingAckCb(EasyLink_Status status);

static void finishedSendingSensorContentCb(EasyLink_Status status);

static void recievedAckOnContentCb(EasyLink_RxPacket *rxPacket,
                                   EasyLink_Status status);

static void timeoutOnContentAckCb();

static void processContentTimerCb();


static void finishedSendingSensorContentAgainCb(EasyLink_Status status);

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smac_init(void *sem)
{
    macSem = sem;
}

void Smac_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smacEvents & SMAC_RECIVE_ACK_MAX_RETRY_EVT)
    {
        Util_clearEvent(&smacEvents, SMAC_RECIVE_ACK_MAX_RETRY_EVT);
    }

    if (smacEvents & SMAC_RECIVE_CONTENT_TIMEOUT_EVT)
    {
        Util_clearEvent(&smacEvents, SMAC_RECIVE_CONTENT_TIMEOUT_EVT);
    }

    if (smacEvents & SMAC_RECIVED_ACK_EVT)
    {
        macMcpsDataCnf_t rsp = { 0 };
        MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_success);
        MAC_sendCnfToApp(&rsp);
        Util_clearEvent(&smacEvents, SMAC_RECIVED_ACK_EVT);
    }

    if (smacEvents & SMAC_RECIVED_CONTENT_EVT)
    {
        macMcpsDataInd_t rsp = { 0 };
        MAC_createDataInd(&rsp, &gSmAckContentInfo.pkt, ApiMac_status_success);
        MAC_sendDataIndToApp(&rsp);
        Util_clearEvent(&smacEvents, SMAC_RECIVED_CONTENT_EVT);
    }

    if (smacEvents & SMAC_FINISHED_EVT)
    {

        Util_clearEvent(&smacEvents, SMAC_FINISHED_EVT);
    }
}

void Smac_sendContent(MAC_crsPacket_t *pkt, uint8_t msduHandle)
{
    EasyLink_abort();
    gMsduHandle = msduHandle;
    memset(&gSmAckContentInfo, 0, sizeof(Smac_smAckContent_t));
    memcpy(gSmAckContentInfo.nodeMac, pkt->dstAddr, APIMAC_SADDR_EXT_LEN);

    uint8_t pBuf[CRS_PKT_MAX_SIZE] = { 0 };
    TX_buildBufFromSrct(pkt, pBuf);

    CollectorLink_pendingPckts_t pendingPacket = { 0 };
    memcpy(pendingPacket.content, pBuf, CRS_PKT_MAX_SIZE - 1);
    CollectorLink_setPendingPkts( &pendingPacket);

    TX_sendPacket(pkt, finishedSendingSensorContentCb);
}




//rxdonecb of waiting for content from the collector
void Smac_recviedCollectorContentCb(EasyLink_RxPacket *rxPacket,
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
        pkt.seqRcv = collectorLink.seqRcv;
        pkt.isNeedAck = 0;
        memcpy(pkt.dstAddr, collectorLink.mac, 8);
        memcpy(pkt.srcAddr, sensorPib.mac, 8);
        pkt.len = 0;
//copy ack packet into Node_pendingPckts_t strcut in the field 'content'
        uint8_t pBuf[MAX_BYTES_PAYLOAD] = { 0 };
        TX_buildBufFromSrct(&pkt, pBuf);
        memcpy(collectorLink.pendingPacket.content, pBuf, MAX_BYTES_PAYLOAD); //check what to copy exactly
        CollectorLink_updateCollector(&collectorLink);

        memset(&gSmAckContentInfo.pkt, 0, sizeof(MAC_crsPacket_t));

        MAC_crsPacket_t pktRec = { 0 };

        RX_buildStructPacket(&pktRec, rxPacket->payload);

        memcpy(&gSmAckContentInfo.pkt, &pktRec, sizeof(MAC_crsPacket_t));
//send ack with cb of 'finishedSendingAckCb'
        TX_sendPacket(&pkt, finishedSendingAckCb);

        gIsDoneSendingAck = false;
//        Util_setEvent(&smacEvents, SMAC_RECIVED_CONTENT_EVT);
//        Semaphore_post(macSem);
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

        gSmacStateArray[gSmacStateArrayIdx] = SMAC_RECIEVED_CONTENT_AGAIN;
        gSmacStateArrayIdx++;
//check seqRecived num- if it is the smaller by 1 from the seqSent in the struct of CollectorLink_collectorLinkInfo_t then:
        //send ack from  Node_pendingPckts_t strcut in the field 'content'
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        MAC_crsPacket_t pkt = { 0 };
        RX_buildStructPacket(&pkt, rxPacket->payload);
        if ((collectorLink.seqRcv - 1) == pkt.seqSent)
        {
            TX_sendPacket(&collectorLink.pendingPacket.content,
                        finishedSendingAckCb); //check which cb
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
//        Clock_setFunc(gclkMacTaskTimer, processContentTimerCb, NULL);
//        Clock_start(gclkMacTaskTimer);

//Increment the seqSent num in the CollectorLink_collectorLinkInfo_t struct
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        collectorLink.seqSend++;
        CollectorLink_updateCollector(&collectorLink);
//enterRx with cb of 'recviedCollectorContentAgainCb'
        gIsDoneSendingAck = true;
//        SMAC_RECIVED_ACK_EVT
        Util_setEvent(&smacEvents, SMAC_RECIVED_CONTENT_EVT);
        Semaphore_post(macSem);
        RX_enterRx( recviedCollectorContentAgainCb, collectorLink.mac);
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
        RX_enterRx(recievedAckOnContentCb, sensorPib.mac);
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
        Util_setEvent(&smacEvents, SMAC_RECIVED_ACK_EVT);
                Semaphore_post(macSem);
//disable timer that would have timeout if no ack was received
//        Clock_stop(gclkMacTaskTimer);
    }
    else
    {
        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        gSmacStateArrayIdx++;
    }

}
//the sensor send content but didnt recieve ack-retry
static void timeoutOnContentAckCb()
{
    gSmacStateArray[gSmacStateArrayIdx] = SMAC_TIMEOUT_ON_CONTENT;
    gSmacStateArrayIdx++;
//send again from the struct Node_pendingPckts_t with txdonecb of 'finishedSendingSensorContentAgainCb'
    CollectorLink_collectorLinkInfo_t collectorLink;
    CollectorLink_getCollector(&collectorLink);
    TX_sendPacket(&collectorLink.pendingPacket.content,
                  finishedSendingSensorContentAgainCb);

}

//txdonecb of sending content event agin in event of ack timeout
static void finishedSendingSensorContentAgainCb(EasyLink_Status status)
{

    if (status == EasyLink_Status_Success)
    {
        gSmacStateArray[gSmacStateArrayIdx] =
                SMAC_FINISHED_SENDING_RETRY_CONTENT;
        gSmacStateArrayIdx++;
//enterRx with cb of 'recievedAckOnContentCb'
        CollectorLink_collectorLinkInfo_t collectorLink;
        CollectorLink_getCollector(&collectorLink);
        RX_enterRx( recievedAckOnContentCb, sensorPib.mac);
//if retry<MAX_RETRY:
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

static void processContentTimerCb()
{
//    Util_setEvent(&macEvents, MAC_TASK_CONTENT_READY);
//    Semaphore_post(macSemHandle);
}

void Smac_printSensorStateMachine()
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
        case SMAC_TIMEOUT_ON_CONTENT:
            CLI_cliPrintf("\r\nSMAC_TIMEOUT_ON_CONTENT");
            break;

        case SMAC_RECIEVED_CONTENT_AGAIN:
            CLI_cliPrintf("\r\nSMAC_RECIEVED_CONTENT_AGAIN");
            break;

        case SMAC_FINISHED_SENDING_RETRY_CONTENT:
            CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_RETRY_CONTENT");
            break;
        case SMAC_ERROR:
            CLI_cliPrintf("\r\nSMAC_ERROR");
            break;
        default:
            break;
        }
    }
}
