/*
 * node.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */


#include "node.h"

#define NUM_NODES 4

static Node_nodeInfo_t gNodes[NUM_NODES];


void Node_init(){

}

void Node_getNode(uint8_t mac[MAC_SIZE],Node_nodeInfo_t* rspNode);

void Node_updateNode(Node_nodeInfo_t* node);





