/*
 * macTask.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_MACTASK_H_
#define MAC_MACTASK_H_

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define MAC_TASK_CLI_UPDATE_EVT 0x00000001
#define MAC_TASK_TX_DONE_EVT 0x00000002
#define MAC_TASK_MSG_RCV_EVT 0x00000004

#define CRS_PAN_ID 0x11

typedef enum
{
  MAC_COMMAND_DATA,
  MAC_COMMAND_ACK
} MAC_commandId_t;

typedef struct Frame
{
    uint16_t len;
    uint16_t seqSent;
    uint16_t seqRcv;
    uint8_t srcAddr[8];
    uint8_t dstAddr[8];
    MAC_commandId_t commandId;
    uint8_t isNeedAck;
} MAC_crsPacket_t;


extern uint16_t macEvents;


void Mac_init();

void Mac_cliUpdate();




#endif /* MAC_MACTASK_H_ */
