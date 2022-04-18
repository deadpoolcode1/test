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

#include "sm_content_ack.h"
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

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/



/******************************************************************************
 Public Functions
 *****************************************************************************/
//typedef struct FrameBeacon
//{
//
//    MAC_commandId_t commandId;
//    uint8_t srcAddr[8];
//    uint16_t srcAddrShort;
//    uint8_t panId;
//
//} MAC_crsBeaconPacket_t;
//
//typedef struct FrameAsocReq
//{
//
//    MAC_commandId_t commandId;
//    uint8_t srcAddr[8];
//    uint16_t srcAddrShort;
//    uint8_t panId;
//
//} MAC_crsAssocReqPacket_t;
//
//typedef struct FrameAsocRsp
//{
//
//    MAC_commandId_t commandId;
//    uint8_t srcAddr[8];
//    uint16_t srcAddrShort;
//    uint8_t panId;
//    bool isPremited;
//
//} MAC_crsAssocRspPacket_t;

//TODO: ADD INITIAL OF CLOCK. AND START PERIODIC CLOCK.
void Smas_init(void *sem)
{
    macSem = sem;
}

void Smas_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smasEvents & SMAS_ADDED_NEW_NODE_EVT)
    {
        Util_clearEvent(&smasEvents, SMAS_ADDED_NEW_NODE_EVT);
    }

    if (smasEvents & SMAS_SEND_BEACON_EVT)
    {
        //TODO send beacon pkt with txdone cb that will rx on PAN ID.
        Util_clearEvent(&smasEvents, SMAS_SEND_BEACON_EVT);
    }


}

//TODO: CB OF BEACON TIMER: RAISE SMAS_SEND_BEACON_EVT. only if the state now is rx_idle. so change to BEACON state.



