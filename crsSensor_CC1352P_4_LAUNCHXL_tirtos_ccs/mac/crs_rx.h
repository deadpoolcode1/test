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
void RX_enterRx(EasyLink_ReceiveCb cbRx);
void RX_getPacket(MAC_crsPacket_t* pkt);
void RX_getPcktStatus(EasyLink_Status* status);

#endif /* MAC_CRS_RX_H_ */
