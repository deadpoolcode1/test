/*
 * sm_test.c
 *
 *  Created on: 5 Sep 2022
 *      Author: epc_4
 */



/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "sm_test.h"
#include "mac/crs_tx.h"
#include "mac/crs_rx.h"
#include "mac/mac_util.h"
#include "cp_cli.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMT_RUN_NEXT_TEST_EVT 0x0001
#define SMT_FINISHED_EVT 0x0002
#define SMT_FAIL_EVT 0x0004





typedef struct StateMachineTest
{
    uint8_t nodeMac[8];
} Smac_smTest_t;

/******************************************************************************
 Local variables
 *****************************************************************************/

static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;


static void *macSem = NULL;

static uint16_t smtEvents = 0;

static Smac_smTest_t gSmTestInfo = { 0 };


static uint32_t gIdxTest = 0;

static uint8_t gMsduHandle = 0;
static uint32_t gLen = 0;

static volatile bool gIsDone = true;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


static bool sendNextPacket();
static void smacFinishedSendingTestCb(EasyLink_Status status);
static void stopSendingClockCb(xdc_UArg arg);


/******************************************************************************
 Public Functions
 *****************************************************************************/

void Smt_init(void *sem)
{
    macSem = sem;
    Clock_Params_init(&gClkParams);
        gClkParams.period = 0;
        gClkParams.startFlag = FALSE;

        Clock_construct(&gClkStruct, NULL, 11000 / Clock_tickPeriod, &gClkParams);

        gClkHandle = Clock_handle(&gClkStruct);


}

void Smt_process()
{

    if (smtEvents & SMT_RUN_NEXT_TEST_EVT)
    {
        if (gIsDone == false)
        {
            sendNextPacket();

        }
//        if (gLen == 0)
//        {
//            macMcpsDataCnf_t rsp = {0};
//            MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_success);
//            MAC_sendCnfToApp(&rsp);
//
//            MAC_moveToRxIdleState();
//
//            Util_clearEvent(&smtEvents, SMT_RUN_NEXT_TEST_EVT);
//        }
//        else
//        {
//            if (gLen < CRS_PAYLOAD_MAX_SIZE - 200)
//            {
//                gLen = 0;
//                sendNextPacket(gLen);
//            }
//            else
//            {
//                gLen = gLen - CRS_PAYLOAD_MAX_SIZE + 200;
//                sendNextPacket(CRS_PAYLOAD_MAX_SIZE - 200);
//            }
//        }
        Util_clearEvent(&smtEvents, SMT_RUN_NEXT_TEST_EVT);

    }

    if (smtEvents & SMT_FINISHED_EVT)
    {
        macMcpsDataCnf_t rsp = {0};
        MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_success);
        MAC_sendCnfToApp(&rsp);

        MAC_moveToRxIdleState();
        Util_clearEvent(&smtEvents, SMT_FINISHED_EVT);
    }

    if (smtEvents & SMT_FAIL_EVT)
    {
        macMcpsDataCnf_t rsp = { 0 };
        MAC_createDataCnf(&rsp, gMsduHandle, ApiMac_status_denied);
        MAC_sendCnfToApp(&rsp);

        MAC_moveToRxIdleState();
        Util_clearEvent(&smtEvents, SMT_FAIL_EVT);
    }




}

void Smd_startTest(uint32_t time, uint8_t msduHandle)
{

    Clock_setFunc(gClkHandle, stopSendingClockCb, 0);
    //10us per tick. so for 1 ms we need 100 ticks
    Clock_setTimeout(gClkHandle, time*100);
    Clock_start(gClkHandle);
    gIsDone = false;
//    gLen = time;
    gMsduHandle = msduHandle;
    sendNextPacket();
//    bool rsp;
//    if (len < CRS_PAYLOAD_MAX_SIZE - 200)
//    {
//        gLen = 0;
//        rsp = sendNextPacket(len);
//    }
//    else
//    {
//        gLen = gLen - CRS_PAYLOAD_MAX_SIZE + 200;
//        rsp = sendNextPacket(CRS_PAYLOAD_MAX_SIZE - 200);
//    }
//
//    if (rsp == false)
//    {
////        MAC_moveToBeaconState();
//    }


}

static bool sendNextPacket()
{



    MAC_crsPacket_t pkt = { 0 };
    pkt.commandId = MAC_COMMAND_TEST;


    uint8_t dstAddr[10] = {0x2,0x2,0x2,0x2  ,0x2,0x2,0x2,0x2};
    memcpy(pkt.dstAddr, dstAddr, 8);

    memcpy(pkt.srcAddr, collectorPib.mac, 8);



    pkt.isNeedAck = 1;
    int i;
    uint32_t len = CRS_PAYLOAD_MAX_SIZE - 200;
    for (i = 0; i < len; i++)
    {
        pkt.payload[i] = 1;
    }
    pkt.len = len;


    TX_sendPacket(&pkt, smacFinishedSendingTestCb);
    return true;


}



static void smacFinishedSendingTestCb(EasyLink_Status status)
{
    //content sent so wait for ack
    if (status == EasyLink_Status_Success)
    {
        Util_setEvent(&smtEvents, SMT_RUN_NEXT_TEST_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(macSem);
    }
    else
    {
        if (gIsDone == false)
        {
            Util_setEvent(&smtEvents, SMT_FAIL_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(macSem);
        }


    }
}

static void stopSendingClockCb(xdc_UArg arg)
{
    gIsDone = true;
    Util_setEvent(&smtEvents, SMT_FINISHED_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSem);
}




