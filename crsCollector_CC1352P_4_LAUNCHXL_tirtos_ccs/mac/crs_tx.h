/*
 * crs_tx.h
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_CRS_TX_H_
#define MAC_CRS_TX_H_
#include "macTask.h"
#include "easylink/EasyLink.h"

void TX_init(void * semaphore);
void TX_sendPacket(MAC_crsPacket_t* pkt,  EasyLink_TxDoneCb cbTx);
void TX_getPcktStatus(EasyLink_Status* status);
void TX_buildBufFromSrct(MAC_crsPacket_t* pkt, uint8_t *pBuf);
void TX_sendPacketBuf(uint8_t* pkt, uint16_t len, uint8_t *dstAddr, EasyLink_TxDoneCb cbTx);

#endif /* MAC_CRS_TX_H_ */
