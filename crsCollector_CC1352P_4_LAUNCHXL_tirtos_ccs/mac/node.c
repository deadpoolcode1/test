/*
 * node.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#include "node.h"
#include "macTask.h"
#include <ti/sysbios/knl/Semaphore.h>
//if its above 16 nodes, then the memory allocated for the structs of the nodes is above 1k
#define NUM_NODES 4

static Node_nodeInfo_t gNodes[NUM_NODES];
static Node_nodeClocks_t gNodesClocks[NUM_NODES];

static bool macCompare(uint8_t macSrc[MAC_SIZE], uint8_t macDest[MAC_SIZE]);
static bool macPrint(uint8_t mac[MAC_SIZE]);
static Clock_Params gClkParams;
static void *sem;

void Node_init(void *semaphore)
{
    sem = semaphore;
    Clock_Params_init(&gClkParams);
    gClkParams.period = 0;
    gClkParams.startFlag = FALSE;

    memset(gNodes, 0, sizeof(Node_nodeInfo_t) * NUM_NODES);
    memset(gNodesClocks, 0, sizeof(Node_nodeClocks_t) * NUM_NODES);
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        gNodes[i].isVacant = true;
        /*   Construct a one-shot Clock Instance*/
        Clock_construct(&gNodesClocks[i].clkStruct, NULL,
                        11000 / Clock_tickPeriod, &gClkParams);
        gNodesClocks[i].clkHandle = Clock_handle(&gNodesClocks[i].clkStruct);
    }
//    uint8_t mac[8]={0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA};
//    memcpy(gNodes[1].mac,mac,MAC_SIZE);
}



void Node_getNode(uint8_t mac[MAC_SIZE], Node_nodeInfo_t *rspNode)
{

    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            memcpy(rspNode, &(gNodes[i]), sizeof(Node_nodeInfo_t));
        }
        else
        {
            //failure
        }
    }

}

void Node_updateNode(Node_nodeInfo_t *node)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, node[i].mac))
        {
            memcpy(&gNodes[i], node, sizeof(Node_nodeInfo_t));
        }
    }
}
void Node_eraseNode(Node_nodeInfo_t* node)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, node[i].mac))
        {
            memset(&gNodes[i], 0, sizeof(Node_nodeInfo_t));
            gNodes[i].isVacant = true;
            memset(gNodesClocks[i].mac, 0, 8);


            return;
        }
    }
}

void Node_addNode(Node_nodeInfo_t *node)
{
    int i = 0;
    while (i < MAC_SIZE)
    {
        if (gNodes[i].isVacant)
        {
            memcpy(&gNodes[i], node, sizeof(Node_nodeInfo_t));
            gNodes[i].isVacant = false;
            memcpy(gNodesClocks[i].mac, node->mac, MAC_SIZE);
            return;
        }
        i++;
    }
}

void Node_listNodes()
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (!(gNodes[i].isVacant))
        {
            CP_CLI_cliPrintf("\r\nNode #0x%x : ", i);
            macPrint(gNodes[i].mac);
        }
    }
}

void Node_setTimeout(uint8_t mac[MAC_SIZE], Clock_FuncPtr clockFxn,
                     UInt timeout)
{
//    11000/Clock_tickPeriod
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodesClocks[i].mac, mac))
        {
            Clock_setFunc(gNodesClocks[i].clkHandle,clockFxn,NULL);
            Clock_setTimeout(gNodesClocks[i].clkHandle,timeout);
           //                 Clock_start(gNodesClocks[i].clkHandle);
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}

void Node_startTimer(uint8_t mac[MAC_SIZE])
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodesClocks[i].mac, mac))
        {
            Clock_start(gNodesClocks[i].clkHandle);
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}

void Node_stopTimer(uint8_t mac[MAC_SIZE])
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodesClocks[i].mac, mac))
        {
            Clock_stop(gNodesClocks[i].clkHandle);
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}

void Node_setNumRetry(uint8_t mac[MAC_SIZE], uint32_t numRetry)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            gNodes[i].numRetry = numRetry;
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}

void Node_setNumRcvPackets(uint8_t mac[MAC_SIZE], uint32_t numRcvPackets)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            gNodes[i].numRcvPackets = numRcvPackets;
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}
void Node_setNumSendPackets(uint8_t mac[MAC_SIZE], uint32_t numSendPackets)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            gNodes[i].numSendPackets = numSendPackets;
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}
void Node_setSeqSend(uint8_t mac[MAC_SIZE], uint16_t seqSend)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            gNodes[i].seqSend = seqSend;
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}
void Node_setSeqRcv(uint8_t mac[MAC_SIZE], uint16_t seqRcv)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            gNodes[i].seqRcv = seqRcv;
            return;
        }
        else
        {
            //failure to find node with this mac
        }
    }
}

void Node_setPendingPckts(uint8_t mac[MAC_SIZE],Node_pendingPckts_t* pendingPacket)
{
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac, mac))
        {
            memcpy(&(gNodes[i].pendingPacket), pendingPacket, sizeof(Node_pendingPckts_t));
            return;
        }

    }
}


void funcTest()
{
    Util_setEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
    Semaphore_post(sem);
}

static bool macCompare(uint8_t macSrc[MAC_SIZE], uint8_t macDest[MAC_SIZE])
{
    int i = 0;
    while (i < MAC_SIZE)
    {
        if (macSrc[i] != macDest[i])
        {
            return false;
        }
        i++;
    }
    return true;
}

static bool macPrint(uint8_t mac[MAC_SIZE])
{
    int i = 0;
    CP_CLI_cliPrintf("0x");
    while (i < MAC_SIZE)
    {
        CP_CLI_cliPrintf("%x", mac[i]);
        i++;
    }
    return true;
}
