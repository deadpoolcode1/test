/*
 * crs_tx.h
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_CRS_TX_H_
#define MAC_CRS_TX_H_
#include "macTask.h"

void TX_init(void * sem);
void TX_sendPacket(MAC_crsPacket_t* pkt);

#endif /* MAC_CRS_TX_H_ */
