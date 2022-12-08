/*
 * crs_msgs.c
 *
 *  Created on: 14 Sep 2022
 *      Author: epc_4
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "crs_msgs.h"
#include "crs/crs_nvs.h"
#include "application/smsgs.h"
#include "application/sensor.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define MSGS_SEND_NEXT_MSG_EVT 0x0001
#define MSGS_FINISHED_EVT 0x0002
//#define MSGS_SEND_NEXT_MSG_EVT 0x0004

#define FILE_SIZE 4096
#define FILE_NAME "MSGS_FILE"
#define RSP_BUFFER_SIZE (SMSGS_CRS_MSG_LENGTH - 100)






/******************************************************************************
 Local variables
 *****************************************************************************/
static uint8_t gRspBuff[RSP_BUFFER_SIZE] = { 0 };
static uint32_t gRspBuffIdx = 0;

static bool gFinishedSending = true;


static void *appSem = NULL;

static uint16_t msgsEvents = 0;



static uint32_t gFileIdx = 0;
static uint16_t gDstShortAddr = 0;

static volatile bool gIsWriteToFile = false;

static volatile bool gIsDone = true;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


static CRS_retVal_t cleanMsg();
static CRS_retVal_t getMsg(uint8_t *msg);
static CRS_retVal_t addToMsg(uint8_t *msg, uint32_t len);
static CRS_retVal_t sendNextMsg();


/******************************************************************************
 Public Functions
 *****************************************************************************/


CRS_retVal_t Msgs_init(void *sem)
{
    appSem = sem;
}

void Msgs_process(void)
{
    if (msgsEvents & MSGS_SEND_NEXT_MSG_EVT)
    {
        sendNextMsg();


        Util_clearEvent(&msgsEvents, MSGS_SEND_NEXT_MSG_EVT);
    }

    if (msgsEvents & MSGS_FINISHED_EVT)
    {
        cleanMsg();
        gFileIdx = 0;
        gDstShortAddr = 0;
        Util_clearEvent(&msgsEvents, MSGS_FINISHED_EVT);
    }
}

CRS_retVal_t Msgs_addMsg(uint8_t * msg, uint32_t len)
{
    if (gRspBuffIdx + len >= RSP_BUFFER_SIZE)
    {
        gIsWriteToFile = true;
        addToMsg(gRspBuff, gRspBuffIdx);
        memset(gRspBuff, 0, sizeof(gRspBuff));
        gRspBuffIdx = 0;
        //            return CRS_FAILURE;
    }

    memcpy(&gRspBuff[gRspBuffIdx], msg, len);
    gRspBuffIdx = gRspBuffIdx + len;
    return CRS_SUCCESS;
}

CRS_retVal_t Msgs_sendMsgs(uint16_t shortAddr)
{
    gFileIdx = 0;
    gDstShortAddr = shortAddr;
//    gFinishedSending = false;
    return sendNextMsg();
}




void Msgs_sendNextMsgCb()
{
    Util_setEvent(&msgsEvents, MSGS_SEND_NEXT_MSG_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(appSem);
}

void Msgs_sendingMsgFailedCb()
{
    Util_setEvent(&msgsEvents, MSGS_FINISHED_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(appSem);
}

static CRS_retVal_t sendNextMsg()
{
    uint8_t fileContent[FILE_SIZE] = {0};
    CRS_retVal_t ret = getMsg(fileContent);
    if (ret !=CRS_SUCCESS)
    {
        return ret;
    }

    uint8_t msgToSend[SMSGS_CRS_MSG_LENGTH + 200] = {0};

    uint32_t fileLen = strlen(fileContent);
    uint32_t crsMsgLen = SMSGS_CRS_MSG_LENGTH - 100;
    uint32_t remainToSend = fileLen - gFileIdx;
    uint32_t sent = 0;
    if (remainToSend  <= crsMsgLen)
    {
        memcpy(msgToSend, &fileContent[gFileIdx], remainToSend);
        sent = remainToSend;
//        gFinishedSending = true;
        cleanMsg();
        Sensor_sendCrsRsp(gDstShortAddr, msgToSend);
        gFileIdx = 0;
        gDstShortAddr = 0;
//        gFinishedSending = false;

    }
    else
    {
        memcpy(msgToSend, &fileContent[gFileIdx], crsMsgLen);
        sent = crsMsgLen;
//        gFinishedSending = false;
        Sensor_sendCrsInParts(gDstShortAddr, msgToSend);

    }

    gFileIdx = gFileIdx + sent;
    return CRS_SUCCESS;

}

static CRS_retVal_t getMsg(uint8_t *msg)
{
    if (gRspBuffIdx > 0 && gIsWriteToFile == false)
    {
        memcpy(msg, gRspBuff, gRspBuffIdx);
        gRspBuffIdx = 0;
        memset(gRspBuff, 0, sizeof(gRspBuff));
        return CRS_SUCCESS;
    }
    if (gRspBuffIdx==0) {
        return CRS_SUCCESS;
    }
    uint8_t *file = Nvs_readFileWithMalloc(FILE_NAME);
    if (file == NULL)
    {
        return CRS_FAILURE;
    }

    memcpy(msg, file, strlen(file));
    memcpy(&msg[strlen(file)], gRspBuff, gRspBuffIdx);
    free(file);
    return CRS_SUCCESS;
}

static CRS_retVal_t addToMsg(uint8_t *msg, uint32_t len)
{
    return  Nvs_writeFile(FILE_NAME, msg);
}

static CRS_retVal_t cleanMsg()
{
    gIsWriteToFile = false;
    gRspBuffIdx = 0;
    memset(gRspBuff, 0, sizeof(gRspBuff));
    return Nvs_rm(FILE_NAME);
}



