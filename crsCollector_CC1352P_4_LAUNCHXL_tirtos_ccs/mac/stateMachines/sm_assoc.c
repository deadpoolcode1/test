/*
 * sm_assoc.c
 *
 *  Created on: 18 Apr 2022
 *      Author: epc_4
 */

/*
 * sm_rx_idle.c
 *
 *  Created on: 18 Apr 2022
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
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMAS_ADDED_NEW_NODE_EVT 0x0001
#define SMAS_SEND_BEACON_EVT 0x0002
#define SMAS_DEBUG_EVT 0x0004

//SMAS_DEBUG_EVT
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

static uint16_t gNumReq = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void beaconClockCb(xdc_UArg arg);
static void sendBeacon();
static void smasFinishedSendingBeaconCb(EasyLink_Status status);

static void buildBeaconBufFromPkt(MAC_crsBeaconPacket_t *beaconPkt,
                                  uint8_t *beaconBuff);
static void buildAssocReqPktFromBuf(MAC_crsAssocReqPacket_t *beaconPkt,
                                    uint8_t *beaconBuff);
static void finishedSendingAssocRspCb(EasyLink_Status status);

static void smasRecivedAssocRspAckCb(EasyLink_RxPacket *rxPacket,
                                     EasyLink_Status status);

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smas_init(void *sem)
{
    macSem = sem;
    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;

    Clock_construct(&gClkStruct, NULL, 11000 / Clock_tickPeriod, &gClkParams);

    gClkHandle = Clock_handle(&gClkStruct);

//    Clock_setFunc(gClkHandle, beaconClockCb, 0);
//    Clock_setTimeout(gClkHandle, 10000);
//    Clock_start(gClkHandle);

}

void Smas_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smasEvents & SMAS_ADDED_NEW_NODE_EVT)
    {
        macMlmeAssociateInd_t rsp = { 0 };
        MAC_createAssocInd(&rsp, gSmAsocInfo.nodeMac, gSmAsocInfo.shortAddr,
                           ApiMac_status_success);
        MAC_moveToRxIdleState();
        MAC_sendAssocIndToApp(&rsp);
        Util_clearEvent(&smasEvents, SMAS_ADDED_NEW_NODE_EVT);
    }

    if (smasEvents & SMAS_DEBUG_EVT)
    {
        CP_CLI_cliPrintf("\r\nrecived assoc req 0x%x", gNumReq);
        Util_clearEvent(&smasEvents, SMAS_DEBUG_EVT);
    }

    if (smasEvents & SMAS_SEND_BEACON_EVT)
    {
        //TODO send beacon pkt with txdone cb that will rx on PAN ID.
        sendBeacon();
        Util_clearEvent(&smasEvents, SMAS_SEND_BEACON_EVT);
    }

}

bool Smas_isNeedToSendBeacon()
{
    if (smasEvents & SMAS_SEND_BEACON_EVT)
    {
        return true;
    }
    return false;

}

static void sendBeacon()
{
    MAC_crsBeaconPacket_t beaconPkt = { 0 };
    beaconPkt.commandId = MAC_COMMAND_BEACON;
    beaconPkt.panId = collectorPib.panId;
    memcpy(beaconPkt.srcAddr, collectorPib.mac, 8);
    beaconPkt.srcAddrShort = collectorPib.shortAddr;

    uint8_t pBuf[200] = { 0 };
    buildBeaconBufFromPkt(&beaconPkt, pBuf);

    TX_sendPacketBuf(pBuf, sizeof(MAC_crsBeaconPacket_t), NULL,
                     smasFinishedSendingBeaconCb);
}

static void smasFinishedSendingBeaconCb(EasyLink_Status status)
{
//content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
//        Clock_setFunc(gClkHandle, beaconClockCb, 0);
//        Clock_setTimeout(gClkHandle, 10000);
//        Clock_start(gClkHandle);
        Util_setEvent(&smasEvents, SMAS_DEBUG_EVT);
        Semaphore_post(macSem);

        RX_enterRx(Smri_recivedPcktCb, collectorPib.mac);
    }

    else
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}

void Smac_recivedAssocReqCb(EasyLink_RxPacket *rxPacket, EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
        MAC_crsAssocReqPacket_t pktRec = { 0 };
        buildAssocReqPktFromBuf(&pktRec, rxPacket->payload);

        if (pktRec.commandId == MAC_COMMAND_ASSOC_REQ)
        {
            memset(&gSmAsocInfo, 0, sizeof(Smas_smAsoc_t));
            memcpy(gSmAsocInfo.nodeMac, pktRec.srcAddr, 8);

            Util_setEvent(&smasEvents, SMAS_DEBUG_EVT);
            gNumReq++;
            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
            Node_nodeInfo_t node = { 0 };
            memcpy(node.mac, pktRec.srcAddr, 8);
//            node.shortAddr = pktRec.srcAddrShort;
            int rsp = Node_addNode(&node);
            MAC_crsAssocRspPacket_t pktRsp = { 0 };

            if (rsp != -1)
            {
                gSmAsocInfo.shortAddr = rsp;

                pktRsp.commandId = MAC_COMMAND_ASSOC_RSP;
                memcpy(pktRsp.srcAddr, collectorPib.mac, 8);
                pktRsp.dstShortAddr = rsp;
                pktRsp.isPremited = true;
            }
            else
            {
                pktRsp.commandId = MAC_COMMAND_ASSOC_RSP;
                memcpy(pktRsp.srcAddr, collectorPib.mac, 8);
                pktRsp.dstShortAddr = rsp;
                pktRsp.isPremited = false;
            }

            TX_sendPacketBuf(((uint8_t*) (&pktRsp)),
                             sizeof(MAC_crsAssocRspPacket_t), pktRec.srcAddr,
                             finishedSendingAssocRspCb);
//            RX_enterRx(finishedSendingAssocRspCb, collectorPib.mac);

        }
        else
        {
//            RX_enterRx(smacRecivedAssocReqCb, collectorPib.mac);
        }

    }
    else
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
    }
}

static void finishedSendingAssocRspCb(EasyLink_Status status)
{
//content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {

        RX_enterRx(smasRecivedAssocRspAckCb, collectorPib.mac);
    }

    else
    {
//        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
//        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}

static void smasRecivedAssocRspAckCb(EasyLink_RxPacket *rxPacket,
                                     EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {

        Util_setEvent(&smasEvents, SMAS_ADDED_NEW_NODE_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
    }
    else
    {
        //        gSmacStateArray[gSmacStateArrayIdx] = SMAC_ERROR;
        //        gSmacStateArrayIdx++;
        //        gCbCcaFailed();

    }
}

static void buildBeaconBufFromPkt(MAC_crsBeaconPacket_t *beaconPkt,
                                  uint8_t *beaconBuff)
{
    memcpy(beaconBuff, (uint8_t*) beaconPkt, sizeof(MAC_crsBeaconPacket_t));
}

static void buildAssocReqPktFromBuf(MAC_crsAssocReqPacket_t *beaconPkt,
                                    uint8_t *beaconBuff)
{
    memcpy(beaconPkt, (uint8_t*) beaconBuff, sizeof(MAC_crsAssocReqPacket_t));

//    beaconPkt = (MAC_crsAssocReqPacket_t*) beaconBuff;
}

//xdc_UArg
//TODO: CB OF BEACON TIMER: RAISE SMAS_SEND_BEACON_EVT. only if the state now is rx_idle. so change to BEACON state.

static void beaconClockCb(xdc_UArg arg)
{
    Util_setEvent(&smasEvents, SMAS_SEND_BEACON_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSem);
}

