/*
 * collectorLink.h
 *
 *  Created on: 5 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_COLLECTORLINK_H_
#define MAC_COLLECTORLINK_H_

#define MAC_SIZE 8
#include <ti/sysbios/knl/Clock.h>

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
uint8_t *content;
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
}Node_nodeInfo_t; //18 bytes in total



void CollectorLink_init();

void CollectorLink_getCollector(Node_nodeInfo_t *rspNode);

void CollectorLink_updateCollector(Node_nodeInfo_t* node);

void CollectorLink_printCollector();


void CollectorLink_setTimeout(Clock_FuncPtr clockFxn,UInt timeout);

void CollectorLink_startTimer();


void CollectorLink_setNumRcvPackets(uint32_t numRcvPackets);
void CollectorLink_setNumSendPackets(uint32_t numSendPackets);
void CollectorLink_setSeqSend(uint16_t seqSend);
void CollectorLink_setSeqRcv(uint16_t seqRcv);

void funcTest();


#endif /* MAC_COLLECTORLINK_H_ */
