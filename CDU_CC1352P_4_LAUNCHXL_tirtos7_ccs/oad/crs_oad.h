/*
 * crs_oad.h
 *
 *  Created on: 2 ????? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_OAD_CRS_OAD_H_
#define APPLICATION_CRS_OAD_CRS_OAD_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs.h"
#include "oad/native_oad/oad_protocol.h"
#include "oad/RadioProtocol.h"


/*!
 * OAD User defined image type begin
 *
 * This is the beginning of the range of image types reserved for the user.
 * OAD applications will not attempt to use or load images with this type
 */
#define OAD_IMG_TYPE_USR_BEGIN            16


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Oad_init(void *sem);
CRS_retVal_t Oad_checkImgEnvVar();
CRS_retVal_t Oad_process();
CRS_retVal_t Oad_distributorUartRcvImg(bool updateUartImgIsReset);
CRS_retVal_t Oad_distributorSendImg(uint16_t dstAddr,bool isReset);
CRS_retVal_t Oad_distributorGetFwVer();
CRS_retVal_t Oad_parseOadPkt( uint8_t* incomingPacket);
CRS_retVal_t Oad_distributorSendTargetFwVerReq(uint16_t dstAddr);
CRS_retVal_t Oad_targetReset(uint16_t dstAddr);
CRS_retVal_t Oad_flashFormat();
void* oadRadioAccessAllocMsg(uint32_t msgLen);


#endif /* APPLICATION_CRS_OAD_CRS_OAD_H_ */
