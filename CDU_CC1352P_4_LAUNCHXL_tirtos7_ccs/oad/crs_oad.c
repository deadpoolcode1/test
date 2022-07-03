/*
 * crs_oad.c
 *
 *  Created on: 2 ????? 2022
 *      Author: nizan
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "oad/crs_oad.h"
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include "application/crs/crs.h"
#include "application/crs/crs_cli.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>
#include <ti/drivers/GPIO.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include "crs_global_defines.h"
#include "application/collector.h"
#include "oad/native_oad/oad_protocol.h"
#include "oad/RadioProtocol.h"
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include "application/crs/crs_env.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define OAD_UART_NEXT_BLOCK_EVT 0x0001
#define OAD_RF_NEXT_BLOCK_EVT 0x0002
#define OAD_RF_FINISHED_OAD_EVT 0x0004
#define ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT 0x0008
#define ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT 0x00010
#define ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT 0x0020
#define ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT 0x0040

/******************************************************************************
 Local variables
 *****************************************************************************/
static Semaphore_Handle collectorSem;
static uint16_t Oad_events = 0;
static bool oad_uart_img_success;
static uint16_t oadClientAddr = 0;
static bool gIsReset=false;
static bool gUpdateUartImgIsReset=false;


/*!
 OAD block variables.
 */
/*static*/uint16_t oadBNumBlocks = 0;//total blocks to send
/*static*/uint16_t oadBlock = 0;//current block sending
static bool oadInProgress = false;
static UART_Handle uartHandle;

/*!
 * Clock for OAD abort
 */
Clock_Struct oadAbortTimeoutClock; /* not static so you can see in ROV */
static Clock_Handle oadAbortTimeoutClockHandle;
static void oadSendBlock(UArg arg0);


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static OADProtocol_Status_t oadRadioAccessPacketSend(void *pDstAddr,
                                                     uint8_t *pMsgPayload,
                                                     uint32_t msgLen);
static void fwVersionRspCb(void *pSrcAddr, char *fwVersionStr);

static void oadImgIdentifyRspCb(void *pSrcAddr, uint8_t status);
static void oadBlockReqCb(void *pSrcAddr, uint8_t imgId, uint16_t blockNum,
                          uint16_t multiBlockSize);
static void oadResetRspCb(void *pSrcAddr);
static void getNextBlock(uint16_t blkNum, uint8_t *oadBlockBuff);
/******************************************************************************
 Callback tables
 *****************************************************************************/

static OADProtocol_RadioAccessFxns_t oadRadioAccessFxns = {
        oadRadioAccessAllocMsg, oadRadioAccessPacketSend };

static OADProtocol_MsgCBs_t oadMsgCallbacks = {
/*! Incoming FW Req */
NULL,
/*! Incoming FW Version Rsp */
fwVersionRspCb,
/*! Incoming Image Identify Req */
NULL,
/*! Incoming Image Identify Rsp */
oadImgIdentifyRspCb,
/*! Incoming OAD Block Req */
oadBlockReqCb,
/*! Incoming OAD Block Rsp */
NULL,
/*! Incoming OAD Reset Req */
NULL,
/*! Incoming OAD Reset Rsp */
oadResetRspCb};

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Oad_init(void *sem)
{
    collectorSem = sem;
    oad_uart_img_success=false;
    OADProtocol_Params_t OADProtocol_params;
    CRS_retVal_t status = CRS_FAILURE;

    /* Create clock object which is used for sending OAD blocks in timed callback */
    Clock_Params clkParams;
    clkParams.period = 0;
    clkParams.startFlag = FALSE;
    Clock_construct(&oadAbortTimeoutClock, oadSendBlock, 1,
                        &clkParams);
    oadAbortTimeoutClockHandle = Clock_handle(&oadAbortTimeoutClock);


    OADProtocol_Params_init(&OADProtocol_params);
    OADProtocol_params.pRadioAccessFxns = &oadRadioAccessFxns;

    if (OADProtocol_params.pRadioAccessFxns->pfnRadioAccessAllocMsg == NULL)
    {
        return status;
    }

    OADProtocol_params.pProtocolMsgCallbacks = &oadMsgCallbacks;

    OADProtocol_open(&OADProtocol_params);
    Oad_checkImgEnvVar();

    return CRS_SUCCESS;

}
CRS_retVal_t Oad_checkImgEnvVar(){
    OADStorage_init();
    /* get Available FW version*/
    OADStorage_imgIdentifyPld_t remoteAppImageId;
    /* get Available FW version*/
    OADStorage_imgIdentifyRead(OAD_IMG_TYPE_USR_BEGIN, &remoteAppImageId);
    if(remoteAppImageId.softVer[0]=='C'){
        char envFile[1024] = { 0 };
        Env_read("img", envFile);
        uint32_t imgPrev = strtol(envFile + strlen("img="),NULL,10);
        char ver[4]={0};
        memcpy(ver,&remoteAppImageId.softVer[1],3);
        uint32_t imgExtFlash=strtol(ver,NULL,10);
        //if this is a collector img - update the img env var
        if ((imgExtFlash>imgPrev)) {
            char currImg[500]={0};
            sprintf(currImg,"img=%s",ver);
            Env_write(currImg);
        }
    }
    OADStorage_close();
    return CRS_SUCCESS;
}


CRS_retVal_t Oad_process()
{
    /* Is it time to send the next sensor data message? */
    if (Oad_events & OAD_UART_NEXT_BLOCK_EVT)
    {
        /* allocate buffer for block + block number */
        uint8_t blkData[OADStorage_BLOCK_SIZE] = { 0 };

        /* is last block? */
        if (oadBlock < oadBNumBlocks)
        {
            uint16_t sentBlockNum;

            /* get block */
            getNextBlock(oadBlock, blkData);

            sentBlockNum = ((uint16_t) (((blkData[0]) & 0x00FF)
                    + (((blkData[1]) & 0x00FF) << 8)));

            if (sentBlockNum == oadBlock)
            {
                /* write block */
                OADStorage_imgBlockWrite(oadBlock, blkData,
                OADStorage_BLOCK_SIZE);
                oadBlock++;
            }

            /* set event to get next block */
            Util_setEvent(&Oad_events, OAD_UART_NEXT_BLOCK_EVT);
            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(collectorSem);
        }
        else
        {
            oad_uart_img_success = false;

            /* end available fw update */
            oadInProgress = false;

            /*
             * Check that CRC is correct and mark the image as new
             * image to be booted in to by BIM on next reset
             */
            if (OADStorage_imgFinalise() == OADStorage_Status_Success)
            {
                oad_uart_img_success = true;
            }

            /* get Available FW version*/
               OADStorage_imgIdentifyPld_t remoteAppImageId;
               /* get Available FW version*/
               OADStorage_imgIdentifyRead(OAD_IMG_TYPE_USR_BEGIN, &remoteAppImageId);
               /* Close resources */
               OADStorage_close();
               UART_close(uartHandle);
               CLI_init(false);
               Oad_distributorGetFwVer();
               CLI_cliPrintf("img loaded successfully");
               CLI_startREAD();
               oadBlock=0;
               if(remoteAppImageId.softVer[0]=='C'){
                   //if the cli command requested reset-perform reset
                   if (gUpdateUartImgIsReset) {
                       //TODO printing
                       SysCtrlSystemReset();
                }
               }

            /* Clear the event */
            Util_clearEvent(&Oad_events, OAD_UART_NEXT_BLOCK_EVT);
        }
    }

    if (Oad_events & OAD_RF_NEXT_BLOCK_EVT) {
        //last block
        if (oadBlock==(oadBNumBlocks)) {
            /* Clear the event */
           Util_clearEvent(&Oad_events, OAD_RF_NEXT_BLOCK_EVT);
        }else{

            uint8_t blockBuf[OADStorage_BLOCK_SIZE - OADStorage_BLK_NUM_HDR_SZ] = { 0 };
            /* read a block from Flash */
            OADStorage_imgBlockRead(oadBlock, blockBuf);
            OADProtocol_Status_t status= OADProtocol_sendOadImgBlockRsp(&oadClientAddr, 0, oadBlock, blockBuf);
            if (status==OADProtocol_Failed) {
                UART_close(uartHandle);
                CLI_init(false);
                CLI_startREAD();
                OADStorage_close();
                oadInProgress=false;
                oadBNumBlocks= 0;
                oadBlock=0;
                CLI_cliPrintf("\r\nsendOadImgBlockRsp Failed");
                   }
            //CLI_cliPrintf("\r\n------------------\r\nsending\r\nclient Addr: %x\r\noadBlock: %d\r\noadBNumBlocks: %d\r\n------------------",oadClientAddr,oadBlock,oadBNumBlocks);
            oadBlock++;

            /* Clear the event */
            Util_clearEvent(&Oad_events, OAD_RF_NEXT_BLOCK_EVT);
        }
    }

    if (Oad_events & OAD_RF_FINISHED_OAD_EVT) {
        CLI_init(false);
        CLI_cliPrintf("\r\noad completed");
        CLI_startREAD();
//        Oad_distributorSendTargetFwVerReq(oadClientAddr);
        if (gIsReset) {
            Oad_targetReset(oadClientAddr);
        }
        OADStorage_close();
        oadInProgress=false;
        oadBNumBlocks= 0;
        oadBlock=0;
        /* Clear the event */
        Util_clearEvent(&Oad_events, OAD_RF_FINISHED_OAD_EVT);
    }
}
/*!
 Update Available FW

 Public function defined in oad_server.h
 */
CRS_retVal_t Oad_distributorUartRcvImg(bool updateUartImgIsReset)
{
    gUpdateUartImgIsReset=updateUartImgIsReset;
    CRS_retVal_t status = CLI_close();
    if (status == CRS_FAILURE)
    {
        return CRS_FAILURE;
    }

    UART_Params uartParams;
    uint8_t imgMetaData[sizeof(OADStorage_imgIdentifyPld_t)];

    if (!oadInProgress)
    {
        /* initialize UART */
        UART_Params_init(&uartParams);
        uartParams.writeDataMode = UART_DATA_BINARY;
        uartParams.readDataMode = UART_DATA_BINARY;
        uartParams.readReturnMode = UART_RETURN_FULL;
        uartParams.readEcho = UART_ECHO_OFF;
        uartParams.baudRate = 115200;
        uartHandle = UART_open(CONFIG_DISPLAY_UART, &uartParams);

        /* get image header */
        UART_read(uartHandle, imgMetaData, sizeof(OADStorage_imgIdentifyPld_t));

        OADStorage_init();

        /* adjust the image type so that the image is stored in the User Image space */
        imgMetaData[IMG_TYPE_OFFSET] = OAD_IMG_TYPE_USR_BEGIN;

        oadBNumBlocks = OADStorage_imgIdentifyWrite(imgMetaData);
        oadBlock = 0;

        /* set event to get next block */
        Util_setEvent(&Oad_events, OAD_UART_NEXT_BLOCK_EVT);
        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(collectorSem);
    }

}

CRS_retVal_t Oad_distributorGetFwVer()
{

    /* get Available FW version*/
    OADStorage_imgIdentifyPld_t remoteAppImageId;
    OADStorage_init();
    /* get Available FW version*/
    oad_uart_img_success=OADStorage_imgIdentifyRead(OAD_IMG_TYPE_USR_BEGIN, &remoteAppImageId);
    if (oad_uart_img_success) {
        switch (remoteAppImageId.softVer[0]) {
            case 'C':
                CLI_cliPrintf("\r\nCollector");
                break;
            case 'S':
                CLI_cliPrintf("\r\nSensor");
                break;
        }
        CLI_cliPrintf(" img loaded successfully sv:%c%c%c bv:%02x",remoteAppImageId.softVer[1], remoteAppImageId.softVer[2],remoteAppImageId.softVer[3], remoteAppImageId.bimVer);
    }else{
        CLI_cliPrintf("\r\nfailed to write img from UART");
    }
    OADStorage_close();
}



CRS_retVal_t Oad_parseOadPkt(uint8_t* incomingPacket){

    OADProtocol_ParseIncoming(&oadClientAddr, (incomingPacket));
}

/*!
 Initiate OAD.

 Public function defined in oad_server.h
 */
 CRS_retVal_t Oad_distributorSendImg(uint16_t dstAddr,bool isReset)
{
     OADStorage_imgIdentifyPld_t remoteImgId;
     oadClientAddr = dstAddr;
     gIsReset=isReset;
    if(!oadInProgress)
    {
        CRS_retVal_t status = CLI_close();
        if (status == CRS_FAILURE){
            return CRS_FAILURE;
        }
        oadInProgress = true;

        OADStorage_init();

        /* get num blocks and setup OADStorage to store in user image region */
        oadBNumBlocks = OADStorage_imgIdentifyRead(OAD_IMG_TYPE_USR_BEGIN, &remoteImgId);

        /*
         * Hard code imgId to 0 - its not used in this
         * implementation as there is only 1 image available
         */
        OADProtocol_Status_t oadStatus = OADProtocol_sendImgIdentifyReq(&dstAddr, 0, (uint8_t*)&remoteImgId);
        if(oadStatus==OADProtocol_Failed || oadBNumBlocks == 0)
        {
            /* issue with image in ext flash */
            oadInProgress = false;
            oadBNumBlocks = 0;
            CLI_init(false);
            CLI_startREAD();
            return CRS_FAILURE;
        }
        return CRS_SUCCESS;

//        return oadBNumBlocks;
    }else{
        CLI_cliPrintf("\r\noadInProgress already");
        return CRS_FAILURE;
    }
}



CRS_retVal_t Oad_distributorSendTargetFwVerReq(uint16_t dstAddr){
    OADProtocol_Status_t status= OADProtocol_sendFwVersionReq(&dstAddr);
    if (status==OADProtocol_Failed) {
        oadInProgress=false;
        oadBlock=0;
        CLI_startREAD();
    }
}

CRS_retVal_t Oad_targetReset(uint16_t dstAddr){
    OADProtocol_sendOadResetReq(&dstAddr);
}

CRS_retVal_t Oad_flashFormat(){
    OADStorage_init();
    OADStorage_flashErase();
    OADStorage_close();
}



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

/******************************************************************************
 Local Functions
 *****************************************************************************/
/*!
 * @brief      Radio access function for OAD module to send messages
 */
static OADProtocol_Status_t oadRadioAccessPacketSend(void *pDstAddr,
                                                     uint8_t *pMsgPayload,
                                                     uint32_t msgLen)
{
    OADProtocol_Status_t status = OADProtocol_Failed;
    Collector_status_t collectorStatus=Collector_status_invalid_state;
    uint8_t *pMsg;
    uint16_t dstAddr = (uint16_t) *((uint16_t*) pDstAddr);
    /*
     * buffer should have been allocated with oadRadioAccessAllocMsg,
     * so 2 byte before the oad msg buffer was allocated for the source
     * addr and Packet ID. Source addr will be filled in by
     * ConcentratorRadioTask_sendNodeMsg
     */
    pMsg = pMsgPayload - 2;
    pMsg[RADIO_PACKET_PKTTYPE_OFFSET] = RADIO_PACKET_TYPE_OAD_PACKET;

    collectorStatus=Collector_oadSendMsg(&dstAddr, pMsg, msgLen+ 2);
    if (collectorStatus==Collector_status_success) {
        status=OADProtocol_Status_Success;
    }
//    ConcentratorRadioTask_sendNodeMsg( (uint8_t*) pDstAddr, pMsg, msgLen + 2);

//free the memory allocated in oadRadioAccessAllocMsg
    CRS_free(pMsg);

    return status;
}

/*!
 * @brief      FW version response callback from OAD module
 */
static void fwVersionRspCb(void *pSrcAddr, char *fwVersionStr)
{
    uint16_t srcAddr = (uint16_t) *((uint16_t*) pSrcAddr);
    CLI_cliPrintf("\r\nclient 0x%x %s",srcAddr,fwVersionStr);
    CLI_startREAD();
//    ConcentratorTask_updateNodeFWVer((uint8_t) *((uint8_t*) pSrcAddr),
//                                     fwVersionStr);
}

/*!
 * @brief      Image Identify response callback from OAD module
 */
static void oadImgIdentifyRspCb(void *pSrcAddr, uint8_t status)
{
    if (status) {
//        CLI_cliPrintf("\r\noadImgIdentifyRspCb success");
        Util_setEvent(&Oad_events, OAD_RF_NEXT_BLOCK_EVT);
        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(collectorSem);
    }else{
        CLI_cliPrintf("\r\noadImgIdentifyRspCb failure");
    }
//    ConcentratorTask_updateNodeOadStatus(ConcentratorTask_NodeOadStatus_InProgress);
}

/*!
 * @brief      Image block request callback from OAD module
 */
static void oadBlockReqCb(void *pSrcAddr, uint8_t imgId, uint16_t blockNum,
                          uint16_t multiBlockSize)
{
//    CLI_cliPrintf("\r\noadBlockReqCb #%d",blockNum);

    (void) imgId;
    if (oadInProgress)
    {
        //if the block was sent successfully, move on to the next block
        if (blockNum==(oadBlock)) {
            /* give 5s grace for the flash erase delay*/
            if (blockNum==1) {
                Clock_setTimeout(oadAbortTimeoutClockHandle,(3000*1000) / Clock_tickPeriod);
            }else{
                Clock_setTimeout(oadAbortTimeoutClockHandle,(1000) / Clock_tickPeriod);
            }
           /* start timer */
           Clock_start(oadAbortTimeoutClockHandle);
        }
        //if its last block
        if (blockNum == (oadBNumBlocks))
        {
            /* OAD complete */
            oadInProgress = false;
            Util_setEvent(&Oad_events, OAD_RF_FINISHED_OAD_EVT);
            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(collectorSem);
        }
        else
        {
//            /* restart timeout in case of abort */
//            Clock_stop(oadAbortTimeoutClockHandle);
//
//            /* give 5s grace for the flash erase delay*/
//            Clock_setTimeout(oadAbortTimeoutClockHandle,
//                             (5000 * 1000) / Clock_tickPeriod);
//
//            /* start timer */
//            Clock_start(oadAbortTimeoutClockHandle);
        }
    }
}


static void oadResetRspCb(void *pSrcAddr)
{
    CLI_cliPrintf("client 0x%x reseted",pSrcAddr);
//    ConcentratorTask_setNodeOadStatusReset();
}


/*!
 * @brief      Get next block of available FW image from UART
 */
static void getNextBlock(uint16_t blkNum, uint8_t *oadBlockBuff)
{
    uint8_t blkNumLower = blkNum & 0xFF;
    uint8_t blkNumUpper = (blkNum & 0xFF00) >> 8;

    UART_write(uartHandle, &blkNumUpper, 1);
    UART_write(uartHandle, &blkNumLower, 1);

    UART_read(uartHandle, oadBlockBuff, OADStorage_BLOCK_SIZE);
}

static void oadSendBlock(UArg arg0){
    /* set event to get next block */
    Util_setEvent(&Oad_events, OAD_RF_NEXT_BLOCK_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

