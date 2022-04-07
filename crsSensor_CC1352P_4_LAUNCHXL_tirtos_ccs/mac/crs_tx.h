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
#include "crs_rx.h"

void TX_init(void * semaphore);
void TX_sendPacket(MAC_crsPacket_t* pkt,uint8_t dstMac[8] ,EasyLink_TxDoneCb cbTx);
void TX_getPcktStatus(EasyLink_Status* status);
void txDoneCbAckListen(EasyLink_RxPacket * rxPacket, EasyLink_Status status);
void TX_buildBufFromSrct(MAC_crsPacket_t* pkt, uint8_t *pBuf);
#endif /* MAC_CRS_TX_H_ */
