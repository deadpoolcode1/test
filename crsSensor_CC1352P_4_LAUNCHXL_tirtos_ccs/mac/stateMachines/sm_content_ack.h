/*
 * sm_content_ack.h
 *
 *  Created on: 13 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_SM_CONTENT_ACK_H_
#define MAC_SM_CONTENT_ACK_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "macTask.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void Smac_init(void *sem);
void Smac_process();
void Smac_sendContent(MAC_crsPacket_t *pkt, uint8_t msduHandle);
void Smac_printSensorStateMachine();

void Smac_recviedCollectorContentCb(EasyLink_RxPacket *rxPacket,
                                      EasyLink_Status status);

#endif /* MAC_SM_CONTENT_ACK_H_ */
