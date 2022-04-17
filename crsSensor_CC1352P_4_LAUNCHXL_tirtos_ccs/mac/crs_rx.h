/*
 * crs_rx.h
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_CRS_RX_H_
#define MAC_CRS_RX_H_

#include "macTask.h"
#include "easylink/EasyLink.h"

void RX_init(void * semaphore);
//if cbRx is null then default callback.
void RX_enterRx(EasyLink_ReceiveCb cbRx, uint8_t dstAddr[8]);
void RX_getPacket(MAC_crsPacket_t* pkt);
void RX_getPcktStatus(EasyLink_Status* status);
void RX_buildStructPacket(MAC_crsPacket_t* pkt, uint8_t *pcktBuff );

#endif /* MAC_CRS_RX_H_ */
