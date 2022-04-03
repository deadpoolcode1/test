/*
 * node.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_NODE_H_
#define MAC_NODE_H_

#define MAC_SIZE 8
#include <stdint.h>
#include <stdbool.h>

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
}Node_nodeInfo_t; //18 bytes in total



void Node_init();

void Node_getNode(uint8_t mac[MAC_SIZE],Node_nodeInfo_t* rspNode);

void Node_updateNode(Node_nodeInfo_t* node);




#endif /* MAC_NODE_H_ */
