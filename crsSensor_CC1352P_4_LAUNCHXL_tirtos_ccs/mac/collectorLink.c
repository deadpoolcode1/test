/*
 * collectorLink.c
 *
 *  Created on: 5 Apr 2022
 *      Author: epc_4
 */

#include "mac/collectorLink.h"
#include "macTask.h"
#include <ti/sysbios/knl/Semaphore.h>

#define NUM_NODES 1

static CollectorLink_collectorLinkInfo_t gNodes[NUM_NODES] = {0};
static CollectorLink_CollectorLinkClocks_t gNodesClocks[NUM_NODES];
static Clock_Params gClkParams;
static void *sem;

static bool macPrint(uint8_t mac[MAC_SIZE]);

void CollectorLink_init(void *semaphore)
{
    sem = semaphore;
    memset(gNodes, 0, sizeof(CollectorLink_collectorLinkInfo_t) * NUM_NODES);
    gNodes[0].isVacant = true;
    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;
    /*   Construct a one-shot Clock Instance*/
    Clock_construct(&gNodesClocks[0].clkStruct, NULL, 11000 / Clock_tickPeriod,
                    &gClkParams);
    gNodesClocks[0].clkHandle = Clock_handle(&gNodesClocks[0].clkStruct);


}

void CollectorLink_getCollector(CollectorLink_collectorLinkInfo_t *rspNode)
{
    if (!(gNodes[0].isVacant))
    {
        memcpy(rspNode, &(gNodes[0]),
               sizeof(CollectorLink_collectorLinkInfo_t));
    }
    else
    {
        //no collector
    }
}

void CollectorLink_updateCollector(CollectorLink_collectorLinkInfo_t *node)
{
    memcpy(&gNodes[0], node, sizeof(CollectorLink_collectorLinkInfo_t));
    gNodes[0].isVacant = false;
}

void CollectorLink_printCollector()
{
    if (!(gNodes[0].isVacant))
    {
        CLI_cliPrintf("\r\nCollector : ");
        macPrint(gNodes[0].mac);
    }
}

void CollectorLink_setTimeout(Clock_FuncPtr clockFxn, UInt timeout)
{
    Clock_setFunc(gNodesClocks[0].clkHandle, clockFxn, NULL);
    Clock_setTimeout(gNodesClocks[0].clkHandle, timeout);
}

void CollectorLink_startTimer()
{
    Clock_start(gNodesClocks[0].clkHandle);
}

void CollectorLink_stopTimer()
{
    Clock_stop(gNodesClocks[0].clkHandle);

}

void CollectorLink_setNumRcvPackets(uint32_t numRcvPackets)
{
    gNodes[0].numRcvPackets = numRcvPackets;

}
void CollectorLink_setNumSendPackets(uint32_t numSendPackets)
{
    gNodes[0].numSendPackets = numSendPackets;

}
void CollectorLink_setSeqSend(uint16_t seqSend)
{
    gNodes[0].seqSend = seqSend;

}
void CollectorLink_setSeqRcv(uint16_t seqRcv)
{
    gNodes[0].seqRcv = seqRcv;

}

void CollectorLink_setPendingPkts(CollectorLink_pendingPckts_t* pendingPkt)
{
memcpy(&(gNodes[0].pendingPacket), pendingPkt, sizeof(CollectorLink_pendingPckts_t));
}
void funcTest()
{
    Util_setEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
    Semaphore_post(sem);
}

static bool macPrint(uint8_t mac[MAC_SIZE])
{
    int i = 0;
    CLI_cliPrintf("0x");
    while (i < MAC_SIZE)
    {
        CLI_cliPrintf("%x", mac[i]);
        i++;
    }
    return true;
}
