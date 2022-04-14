/*
 * sm_discovery.c
 *
 *  Created on: 14 Apr 2022
 *      Author: epc_4
 */




/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "sm_discovery.h"
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"
#include "mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMD_RECIVE_ACK_MAX_RETRY_EVT 0x0001
#define SMD_RECIVED_ACK_EVT 0x0002
#define SMD_FINISHED_EVT 0x0004

#define SMD_DISCOVER_NEXT_NODE_EVT 0x0008


typedef enum
{
    SMAC_RX_IDLE,
    SMAC_SENT_CONTENT,
    SMAC_SENT_CONTENT_AGAIN,

    SMAC_RECIVED_CONTENT_ACK,
    SMAC_RECIVED_CONTENT,
    SMAC_RECIVED_CONTENT_AGAIN,

    SMAC_FINISHED_SENDING_CONTENT_ACK,
    SMAC_NOTIFY_FAIL,
    SMAC_NOTIFY_SUCCESS,
    SMAC_ERROR
} Mac_smStateCodes_t;

typedef struct StateMachineDiscovery
{
    uint8_t nodeMac[8];
    uint8_t contentRspRssi;

} Smac_smDiscovery_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *macSem = NULL;

static uint16_t smdEvents = 0;

static Smac_smDiscovery_t gSmDiscoveryInfo = { 0 };

static Mac_smStateCodes_t gSmacStateArray[100] = { 0 };
static uint32_t gSmacStateArrayIdx = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/



/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smd_init(void *sem)
{
    macSem = sem;
}

void Smd_process()
{
//    SMAC_RECIVE_CONTENT_TIMEOUT_EVT SMAC_RECIVED_ACK SMAC_RECIVED_CONTENT_EVT
    if (smdEvents & SMD_RECIVE_ACK_MAX_RETRY_EVT)
    {
        Util_clearEvent(&smdEvents, SMD_RECIVE_ACK_MAX_RETRY_EVT);
    }

    if (smdEvents & SMD_RECIVED_ACK_EVT)
    {
        Util_clearEvent(&smdEvents, SMD_RECIVED_ACK_EVT);
    }

    if (smdEvents & SMD_DISCOVER_NEXT_NODE_EVT)
    {

        Util_clearEvent(&smdEvents, SMD_DISCOVER_NEXT_NODE_EVT);
    }

    if (smdEvents & SMD_FINISHED_EVT)
    {

        Util_clearEvent(&smdEvents, SMD_FINISHED_EVT);
    }
}

void Smd_startDiscovery()
{
    memset(&gSmDiscoveryInfo, 0, sizeof(Smac_smDiscovery_t));


}


void Smd_printStateMachine()
{

    int i = 0;
    for (i = 0; i < gSmacStateArrayIdx; ++i)
    {
        switch (gSmacStateArray[i])
        {
        case SMAC_RX_IDLE:
            CP_CLI_cliPrintf("\r\nSMAC_RX_IDLE");
            break;
        case SMAC_SENT_CONTENT:
            CP_CLI_cliPrintf("\r\nSMAC_SENT_CONTENT");
            break;
        case SMAC_SENT_CONTENT_AGAIN:
            CP_CLI_cliPrintf("\r\nSMAC_SENT_CONTENT_AGAIN");
            break;
        case SMAC_RECIVED_CONTENT_ACK:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_ACK");

            break;
        case SMAC_RECIVED_CONTENT:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT");

            break;
        case SMAC_FINISHED_SENDING_CONTENT_ACK:
            CP_CLI_cliPrintf("\r\nSMAC_FINISHED_SENDING_CONTENT_ACK");

            break;

        case SMAC_RECIVED_CONTENT_AGAIN:
            CP_CLI_cliPrintf("\r\nSMAC_RECIVED_CONTENT_AGAIN");
            break;
        case SMAC_ERROR:
            CP_CLI_cliPrintf("\r\nSMAC_ERROR");
            break;
        default:
            break;
        }
    }
}
