/******************************************************************************

 @file oad_client.c

 @brief OAD Client

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

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <ti/sysbios/knl/Clock.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/flash.h)

#include "crs_oad_client.h"
#include <oad/native_oad/oad_protocol.h>
#include <oad/native_oad/oad_storage.h>
#include <ti_easylink_oad_config.h>
#include <common/cc26xx/oad/oad_image_header.h>
#include <common/cc26xx/flash_interface/flash_interface.h>
#include <oad/native_oad/oad_image_header_app.h>

#include "oad/native_oad/RadioProtocol.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include "application/crs/crs.h"
#include "application/crs/crs_cli.h"
#include "application/sensor.h"
#include "application/smsgs.h"
#include <ti/sysbios/knl/Task.h>
#include "application/crs/crs_env.h"
#include <ti/sysbios/knl/Task.h>


//oad switch
#include "common/cc26xx/flash_interface/flash_interface.h"
#include <ti/drivers/dpl/HwiP.h>
#include DeviceFamily_constructPath(driverlib/flash.h)
#include "oad/native_oad/oad_image_header_app.h"
#include "oad/native_oad/flash_interface_internal.h"
#include "application/crs/crs_locks.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define OAD_CLIENT_REQ_EVT 0x0001
#define OAD_CLIENT_RSP_POLL_EVT 0x0002
#define OAD_CLIENT_FINISHED_OAD_EVT 0x0004

/*!
 OAD block variables.
 */

static uint16_t oadBNumBlocks = 0;
static uint16_t oadBlock = 0;
static uint16_t oadServerAddr = {0};
static bool oadInProgress = false;

static uint16_t Oad_clientEvents = 0;
static Semaphore_Handle collectorSem;
static bool gIsReset=false;

OADClient_Params_t oadClientParams;

static Clock_Struct oadBlockReqClkStruct;
static Clock_Handle oadBlockReqClkHandle;

static Clock_Struct oadRspPollClkStruct;
static Clock_Handle oadRspPollClkHandle;

/******************************************************************************
 Local function prototypes
 *****************************************************************************/
static void oadFwVersionReqCb(void* pSrcAddr);
#ifndef OAD_U_APP
static void oadImgIdentifyReqCb(void* pSrcAddr, uint8_t imgId, uint8_t *imgMetaData);
static void oadBlockRspCb(void* pSrcAddr, uint8_t imgId, uint16_t blockNum, uint8_t *blkData);
#endif
static void oadResetReqCb(void* pSrcAddr);
void* oadRadioAccessAllocMsg(uint32_t msgLen);
static OADProtocol_Status_t oadRadioAccessPacketSend(void* pDstAddr, uint8_t *pMsg, uint32_t msgLen);

/******************************************************************************
 Callback tables
 *****************************************************************************/

static OADProtocol_RadioAccessFxns_t oadRadioAccessFxns = { oadRadioAccessAllocMsg,
oadRadioAccessPacketSend };

#ifdef OAD_U_APP
/* User app should only respond to fwVersion and oadReset requests */
static OADProtocol_MsgCBs_t oadMsgCallbacks =
    {
      /*! Incoming FW Req */
      oadFwVersionReqCb,
      /*! Incoming FW Version Rsp */
      NULL,
      /*! Incoming Image Identify Req */
      NULL,
      /*! Incoming Image Identify Rsp */
      NULL,
      /*! Incoming OAD Block Req */
      NULL,
      /*! Incoming OAD Block Rsp */
      NULL,
      /*! Incoming OAD Reset Req */
      oadResetReqCb,
      /*! Incoming OAD Reset Rsp */
      NULL,
    };
#else
static
OADProtocol_MsgCBs_t oadMsgCallbacks = {
/*! Incoming FW Req */
oadFwVersionReqCb,
/*! Incoming FW Version Rsp */
NULL,
/*! Incoming Image Identify Req */
oadImgIdentifyReqCb,
/*! Incoming Image Identify Rsp */
NULL,
/*! Incoming OAD Block Req */
NULL,
/*! Incoming OAD Block Rsp */
oadBlockRspCb,
/*! Incoming OAD Reset Req */
oadResetReqCb,
/*! Incoming OAD Reset Rsp */
NULL
, };
#endif


/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t OadClient_init(void *sem)
{
    collectorSem = sem;
    OADClient_Params_t params = { 0 };
    params.oadEvents = Oad_clientEvents;
    OADClient_open(&params);
    Oad_checkImgEnvVar();
}

CRS_retVal_t Oad_checkImgEnvVar(){
    //if this is a sensor img - update the img env var
    if( _imgHdr.fixedHdr.softVer[0]=='S' || _imgHdr.fixedHdr.softVer[0]=='F'){
        char envFile[1024] = { 0 };
        Env_read("img", envFile);
        uint32_t imgPrev = strtol(envFile + strlen("img="),NULL,10);
        char ver[4]={0};
        memcpy(ver,&_imgHdr.fixedHdr.softVer[1],3);
        uint32_t imgExtFlash=strtol(ver,NULL,10);
//        if ((imgExtFlash>imgPrev)) {
            char currImg[500]={0};
            sprintf(currImg,"img=%s",ver);
            Env_write(currImg);
//        }
    }
//    OADStorage_close();
    return CRS_SUCCESS;
}


CRS_retVal_t Oad_parseOadPkt(void* pSrcAddress, uint8_t* incomingPacket){
    uint16_t srcAddr = 0xaabb;//(uint16_t) *((uint16_t*) pSrcAddress);
    oadServerAddr=srcAddr;
    OADProtocol_ParseIncoming(&oadServerAddr, (incomingPacket));
}

/*!
 Initialize this application.

 Public function defined in sensor.h
 */
void OADClient_open(OADClient_Params_t *params)
{
    OADProtocol_Params_t OADProtocol_params = { 0 };
    memcpy(&oadClientParams, params, sizeof(OADClient_Params_t));
    OADProtocol_Params_init(&OADProtocol_params);
    OADProtocol_params.pRadioAccessFxns = &oadRadioAccessFxns;
    OADProtocol_params.pProtocolMsgCallbacks = &oadMsgCallbacks;

    OADProtocol_open(&OADProtocol_params);

//    oadClockInitialize();

//    OADStorage_init();
}

/*!
 Application task processing.

 Public function defined in sensor.h
 */
CRS_retVal_t OadClient_process(void)
{
    /* Is it time to send the next sensor data message? */
    if ( Oad_clientEvents & OAD_CLIENT_REQ_EVT)
    {
//        OADStorage_init();
        CLI_cliPrintf("\roadBlock #%d",oadBlock);
        OADProtocol_sendOadImgBlockReq(&oadServerAddr, 0, oadBlock, 1);
        Util_clearEvent(&Oad_clientEvents, OAD_CLIENT_REQ_EVT);
    }
    if ( Oad_clientEvents & OAD_CLIENT_FINISHED_OAD_EVT)
       {
        oadInProgress=false;
        Locks_enable();
        oadBNumBlocks=0;
        OADStorage_init();
        OADStorage_Status_t status = OADStorage_imgFinalise();
        OADStorage_close();
        if (status == OADStorage_Status_Success) {
            CLI_cliPrintf("\r\nOAD completed successfully");
//            SysCtrlSystemReset();
        }else{
            CLI_cliPrintf("\r\nOADStorage_imgFinalise fail");
        }
        Util_clearEvent(&Oad_clientEvents, OAD_CLIENT_FINISHED_OAD_EVT);
       }
}

/*!
 Flip bit in OAD image header to invalidate the image
 */
void OADClient_invalidateHeader(void)
{
    // Find the byte offset of the imgVld bytes in the OAD image header
    size_t imgVldOffset = (uint8_t*) &(_imgHdr.fixedHdr.imgVld)
            - (uint8_t*) &_imgHdr; //offsetof(_imgHdr, fixedHdr.imgVld);

    // Read the current imgVld value from flash
    uint32_t imgVld = 0;
    readInternalFlash(imgVldOffset, (uint8_t*) &imgVld, sizeof(imgVld));

    // Shift once to change the number of 0 bits from even to odd
    imgVld = imgVld << 1;

    // Write the new imgVld value to the OAD image header in flash
    writeInternalFlash(imgVldOffset, (uint8_t*) &imgVld, sizeof(imgVld));
}


CRS_retVal_t Oad_flashFormat(){
    OADStorage_init();
    OADStorage_flashErase();
    OADStorage_close();
}



CRS_retVal_t Oad_createFactoryImageBackup(){

    uint8_t rtn=   OADStorage_createFactoryImageBackup();
    switch (rtn) {
        case OADStorage_Status_Success:
            return CRS_SUCCESS;
        case OADStorage_FlashError:
            return CRS_FAILURE;
        default:
            return CRS_SUCCESS;
    }


}


CRS_retVal_t Oad_invalidateImg(){
    uint8_t retval =   eraseInternalbimFlashPg(0x0);
        return CRS_SUCCESS;

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
/*!
 * @brief      FW Version request callback
 */
static void oadFwVersionReqCb(void *pSrcAddr)
{
    char fwVersionStr[OADProtocol_FW_VERSION_STR_LEN] = { 0 };
    char bimVerStr[2];
#ifdef FEATURE_SECURE_OAD
    char securityTypeStr[2];
#endif //FEATURE_SECURE_OAD

    memcpy(fwVersionStr, "sv:", 3);
    memcpy(&fwVersionStr[3], _imgHdr.fixedHdr.softVer, 4);
    memcpy(&fwVersionStr[7], " bv:", 4);

    //convert bimVer to string
    bimVerStr[1] =
            (_imgHdr.fixedHdr.bimVer & 0x0F) < 0xA ?
                    ((_imgHdr.fixedHdr.bimVer & 0x0F) + 0x30) :
                    ((_imgHdr.fixedHdr.bimVer & 0x0F) + 0x41);
    bimVerStr[0] =
            ((_imgHdr.fixedHdr.bimVer & 0xF0) >> 8) < 0xA ?
                    (((_imgHdr.fixedHdr.bimVer & 0xF0) >> 8) + 0x30) :
                    (((_imgHdr.fixedHdr.bimVer & 0xF0) >> 8) + 0x41);

    memcpy(&fwVersionStr[11], bimVerStr, 2);

#ifdef FEATURE_SECURE_OAD
    memcpy(&fwVersionStr[13], " st:", 4);

    //convert bimVer to string
    securityTypeStr[1] = (_imgHdr.secVer & 0x0F) < 0xA ?
            ((_imgHdr.secVer & 0x0F) + 0x30):
            ((_imgHdr.secVer & 0x0F) + 0x41);
    securityTypeStr[0] = ((_imgHdr.secVer & 0xF0) >> 8) < 0xA ?
            (((_imgHdr.secVer & 0xF0) >> 8) + 0x30):
            (((_imgHdr.secVer & 0xF0) >> 8) + 0x41);

    memcpy(&fwVersionStr[17], securityTypeStr, 2);
#endif //FEATURE_SECURE_OAD

    //Send response back
    OADProtocol_sendFwVersionRsp(pSrcAddr, fwVersionStr);
}

#ifndef OAD_U_APP
/*!
 * @brief      OAD image identify request callback
 */
static void oadImgIdentifyReqCb(void *pSrcAddr, uint8_t imgId,
                                uint8_t *imgMetaData)
{

//    CLI_cliPrintf("\r\noadImgIdentifyReqCb");
    /*
     * Ignore imgId - its not used in this
     * implementation as there is only 1 image available
     */
    (void) imgId;

    oadServerAddr = *((uint16_t*) pSrcAddr);
    OADStorage_init();
    oadBNumBlocks = OADStorage_imgIdentifyWrite(imgMetaData);
    OADStorage_close();
    oadBlock = 0;

    if (oadBNumBlocks)
    {
        oadInProgress = true;
        Locks_disable();//disable locks process checks, in order for it not make troubles with the spi extFlash
        //Send success response back
        OADProtocol_sendOadIdentifyImgRsp(pSrcAddr, 1);
    }
    else
    {
        //Send fail response back
        OADProtocol_sendOadIdentifyImgRsp(pSrcAddr, 0);
    }
}

/*!
 * @brief      OAD image block response callback
 */
static void oadBlockRspCb(void *pSrcAddr, uint8_t imgId, uint16_t blockNum,
                          uint8_t *blkData)
{
//    CLI_cliPrintf("\r\noadBlockRspCb #%d",blockNum);
//    CLI_cliPrintf("------------------\r\expecting oadBlock: %drecived blockNum: %d\r\n\r\noadBNumBlocks: %d\r\n------------------",oadBlock,blockNum,oadBNumBlocks);
    /*
     * Ignore imgId - its not used in this
     * implementation as there is only 1 image available
     */
    (void) imgId;
    if ((blockNum == oadBlock) && (oadInProgress))
    {
        OADStorage_init();
        oadBlock++;
        OADStorage_Status_t status = OADStorage_imgBlockWrite(blockNum, (blkData), OADStorage_BLOCK_SIZE);
        if (status!=OADStorage_Status_Success) {
            CLI_cliPrintf("\r\nfailed to write to flash");
            OADStorage_close();
            return;
        }
        OADStorage_close();
        //request the next block from the server
        Util_setEvent(&Oad_clientEvents, OAD_CLIENT_REQ_EVT);
        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(collectorSem);
    }else{
        CLI_cliPrintf("\r\ngiven blockNum not good");
    }
    if (oadBlock==(oadBNumBlocks)) {
        Util_setEvent(&Oad_clientEvents, OAD_CLIENT_FINISHED_OAD_EVT);
        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(collectorSem);
    }
}
#endif

/*!
 * @brief      Radio access function for OAD module to send messages
 */
void* oadRadioAccessAllocMsg(uint32_t msgLen)
{
    uint8_t *msgBuffer;

    /*
     * Allocate with 2 byte before the oad msg buffer for
     * addr and packet ID.
     */
    msgBuffer = (uint8_t*) CRS_malloc(msgLen + 2);
    if (msgBuffer == NULL)
    {
        return NULL;
    }
    memset(msgBuffer, 0, msgLen + 2);

    return msgBuffer + 2;
}

/*!
 * @brief      Radio access function for OAD module to send messages
 */
static OADProtocol_Status_t oadRadioAccessPacketSend(void *pDstAddr,
                                                     uint8_t *pMsgPayload,
                                                     uint32_t msgLen)
{
    OADProtocol_Status_t status = OADProtocol_Failed;
    Smsgs_statusValues_t SensorStatus=Smsgs_statusValues_invalid;
    uint8_t *pMsg;
    uint16_t dstAddr = (uint16_t) *((uint16_t*) pDstAddr);
    /*
     * buffer should have been allocated with oadRadioAccessAllocMsg,
     * so 2 byte before the oad msg buffer was allocated for the source
     * addr and Packet ID. Source addr will be filled in by
     * NodeRadioTask_sendOadMsg
     */
    pMsg = pMsgPayload - 2;
    pMsg[RADIO_PACKET_PKTTYPE_OFFSET] = RADIO_PACKET_TYPE_OAD_PACKET;

    SensorStatus= Sensor_sendOadRsp(&dstAddr, pMsg, msgLen + 2);
    if (SensorStatus==Smsgs_statusValues_success) {
        status=OADProtocol_Status_Success;
    }
    //free the memory allocated in oadRadioAccessAllocMsg
    CRS_free(&pMsg);

    return status;
}

/*!
 * @brief      OAD reset request callback
 */
static void oadResetReqCb(void *pSrcAddr)
{
#ifdef OAD_U_APP
    NodeTask_resetUserApp(pSrcAddr);
#else
    //Send response back
    OADProtocol_sendOadResetRsp(pSrcAddr);
    Task_sleep(1000);
    SysCtrlSystemReset();
#endif
}




