/*
 * node.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_NODE_H_
#define MAC_NODE_H_
#include <ti/sysbios/knl/Clock.h>

#define MAC_SIZE 8
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


typedef struct nodeClocks{
    uint8_t mac[MAC_SIZE];
    Clock_Struct clkStruct;
    Clock_Handle clkHandle;
}Node_nodeClocks_t;


typedef struct pendingPckts{
    uint32_t handle;
uint8_t content[256];
uint16_t contentLen;
bool isAckRcv;
bool isContentRcv;
}Node_pendingPckts_t;


typedef struct Node{

Node_pendingPckts_t pendingPacket;
uint32_t numRcvPackets;
uint32_t numSendPackets;
uint16_t seqSend;
uint16_t seqRcv;
uint8_t mac[MAC_SIZE];
//clock
bool istimeout;
uint32_t numRetry;
bool isVacant;
uint16_t shortAddr;
}Node_nodeInfo_t; //18 bytes in total



void Node_init();

void Node_getNode(uint8_t mac[MAC_SIZE],Node_nodeInfo_t* rspNode);

void Node_updateNode(Node_nodeInfo_t* node);

void Node_eraseNode(Node_nodeInfo_t* node);


int Node_addNode(Node_nodeInfo_t* node);

void Node_setTimeout(uint8_t mac[MAC_SIZE], Clock_FuncPtr clockFxn,UInt timeout);

void Node_startTimer(uint8_t mac[MAC_SIZE]);

void Node_stopTimer(uint8_t mac[MAC_SIZE]);

void Node_setNumRcvPackets(uint8_t mac[MAC_SIZE],uint32_t numRcvPackets);
void Node_setNumSendPackets(uint8_t mac[MAC_SIZE],uint32_t numSendPackets);
void Node_setSeqSend(uint8_t mac[MAC_SIZE],uint16_t seqSend);
void Node_setSeqRcv(uint8_t mac[MAC_SIZE],uint16_t seqRcv);
void Node_setPendingPckts(uint8_t mac[MAC_SIZE],Node_pendingPckts_t* pendingPacket);
void Node_setNumRetry(uint8_t mac[MAC_SIZE], uint32_t numRetry);

//void Node_setmac(uint8_t mac[MAC_SIZE],uint8_t mac[MAC_SIZE]);

////clock
//bool istimeout;
//uint32_t numRetry;
//bool isVacant;


void funcTest();

#endif /* MAC_NODE_H_ */
