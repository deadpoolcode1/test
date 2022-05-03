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

#include "sm_rx_idle.h"
#include "mac/node.h"
#include "mac/crs_tx.h"
#include "mac/crs_rx.h"
#include "mac/mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMRI_EVT 0x0001



typedef struct StateMachineAckContent
{
    uint8_t nodeMac[8];
    MAC_crsPacket_t pkt;
    uint8_t contentRsp[256];
    uint16_t contentRspLen;
    uint8_t contentRspRssi;

} Smri_smRxIdle_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

void *macSem = NULL;

static uint16_t smriEvents = 0;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


/******************************************************************************
 Public Functions
 *****************************************************************************/


void Smri_init(void *sem)
{
    macSem = sem;

}
void Smri_process()
{
    if (smriEvents & SMRI_EVT)
        {
            Util_clearEvent(&smriEvents, SMRI_EVT);
        }
}

