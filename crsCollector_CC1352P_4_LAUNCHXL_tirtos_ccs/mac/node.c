/*
 * node.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#include "node.h"

#define NUM_NODES 4

static Node_nodeInfo_t gNodes[NUM_NODES];
static bool macCompare(uint8_t macSrc[MAC_SIZE],uint8_t macDest[MAC_SIZE]);
static bool macPrint(uint8_t mac[MAC_SIZE]);
void Node_init()
{
    memset(gNodes, 0, sizeof(Node_nodeInfo_t) * NUM_NODES);
    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
            gNodes[i].isVacant=true;
    }
//    uint8_t mac[8]={0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA};
//    memcpy(gNodes[1].mac,mac,MAC_SIZE);
}


void Node_getNode(uint8_t mac[MAC_SIZE], Node_nodeInfo_t *rspNode)
{

    int i = 0;
    for (i = 0; i < NUM_NODES; ++i)
    {
        if (macCompare(gNodes[i].mac,mac))
        {
            memcpy(rspNode, &(gNodes[i]), sizeof(Node_nodeInfo_t));
        }
        else
        {
            //failure
        }
    }

}

void Node_updateNode(Node_nodeInfo_t *node){

    int i = 0;
       for (i = 0; i < NUM_NODES; ++i)
       {
           if (macCompare(gNodes[i].mac,node[i].mac))
           {
               memcpy(&gNodes[i],node,sizeof(Node_nodeInfo_t));
           }
       }
}


void Node_addNode(Node_nodeInfo_t* node){
    int i=0;
    while(i<MAC_SIZE){
        if (gNodes[i].isVacant) {
            memcpy(&gNodes[i],node,sizeof(Node_nodeInfo_t));
            gNodes[i].isVacant=false;
            return;
        }
        i++;
    }
}

void Node_listNodes(){
    int i=0;
    for (i = 0; i < NUM_NODES; ++i)
      {
          if (!(gNodes[i].isVacant))
          {
              CLI_cliPrintf("\r\nNode #0x%x : ",i);
              macPrint(gNodes[i].mac);
          }
      }
}



static bool macCompare(uint8_t macSrc[MAC_SIZE],uint8_t macDest[MAC_SIZE]){
    int i=0;
    while(i<MAC_SIZE){
        if (macSrc[i]!=macDest[i]) {
            return false;
        }
        i++;
    }
    return true;
}


static bool macPrint(uint8_t mac[MAC_SIZE]){
    int i=0;
    CLI_cliPrintf("0x");
    while(i<MAC_SIZE){
        CLI_cliPrintf("%x",mac[i]);
        i++;
    }
    return true;
}
