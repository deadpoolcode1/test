/*
 * crs_oad_client.h
 *
 *  Created on: 6 ????? 2022
 *      Author: cellium
 */

#ifndef OAD_NATIVE_OAD_CRS_OAD_CLIENT_H_
#define OAD_NATIVE_OAD_CRS_OAD_CLIENT_H_


/******************************************************************************

 @file oad_client.h

 @brief OAD Client Header

 Group: CMCU LPRF
 Target Device: cc13x2_26x2

 ******************************************************************************

 Copyright (c) 2016-2021, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************


 *****************************************************************************/

#include <ti/sysbios/knl/Event.h>
#include "application/crs/crs.h"
#include "application/crs/crs_cli.h"
#ifdef __cplusplus
extern "C"
{
#endif

/** @brief RF parameter struct
 *  RF parameters are used with the OADClient_open() and OADClient_Params_t() call.
 */
typedef struct {
    Event_Handle eventHandle;               ///< Event handle to post to
    uint32_t oadReqEventBit;                ///< event to post
    uint32_t oadRspPollEventBit;            ///< event to post
    uint16_t oadEvents;
} OADClient_Params_t;

CRS_retVal_t Oad_flashFormat();
CRS_retVal_t Oad_parseOadPkt(void* pSrcAddress, uint8_t* incomingPacket);
CRS_retVal_t OadClient_init(void *sem);
CRS_retVal_t Oad_checkImgEnvVar();
CRS_retVal_t Oad_createFactoryImageBackup();
CRS_retVal_t Oad_invalidateImg();
 /** @brief  Function to open the SOADProtocol module
 *
 *  @param  params      An pointer to OADClient_Params_t structure for initialization
 */
extern void OADClient_open(OADClient_Params_t *params);

 /** @brief  Function to process OAD events
 *
 *  @param  pEvent      Event to process
 */
extern CRS_retVal_t OadClient_process(void);

/** @brief  Function to invalidate the OAD image header of the U-App
 *  Called when the user app is performing a reset to the P-App context
 */
extern void OADClient_invalidateHeader(void);


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif




#endif /* OAD_NATIVE_OAD_CRS_OAD_CLIENT_H_ */
