/*
 * macTask.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>
#include <xdc/runtime/System.h>

#include "macTask.h"
#include "cp_cli.h"
#include "mac_util.h"
#include "node.h"
#include "crs_tx.h"
#include "crs_rx.h"


/* EasyLink API Header files */
#include "easylink/EasyLink.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define MAC_TASK_STACK_SIZE    1024
#define MAC_TASK_PRIORITY      2


/******************************************************************************
 Local variables
 *****************************************************************************/

typedef enum
{
    SMAC_RX_IDLE,
    SMAC_SEND_CONTENT,
    SMAC_WAIT_CONTENT_ACK,
    SMAC_WAIT_CONTENT,
    SMAC_SEND_CONTENT_ACK,
    SMAC_NOTIFY_FAIL,
    SMAC_NOTIFY_SUCCESS,
} Mac_smStateCodes_t;

typedef void (*SmFunc)(void);

typedef struct StateMachineAckContent
{


} Mac_smAckContent_t;


Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

/*! Storage for Events flags */
uint16_t macEvents = 0;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void macFnx(UArg arg0, UArg arg1);
static void processTxDone();
static void processRxDone();
static void stateMachineAckContent(Mac_smStateCodes_t argStateCode);

static void smacRxIdle();
static void smacSendContent();
static void smacWaitContentAck();
static void smacWaitContent();
static void smacSendContentAck();
static void smacNotifyFail();
static void smacNotifySuccess();

SmFunc gSmFpArr[7] = { smacRxIdle, smacSendContent, smacWaitContentAck,
                       smacWaitContent, smacSendContentAck, smacNotifyFail,
                       smacNotifySuccess };


void Mac_init()
{

    Task_Params_init(&macTaskParams);
    macTaskParams.stackSize = MAC_TASK_STACK_SIZE;
    macTaskParams.priority = MAC_TASK_PRIORITY;
    macTaskParams.stack = &macTaskStack;
    macTaskParams.arg0 = (UInt) 1000000;

    Task_construct(&macTask, macFnx, &macTaskParams, NULL);

}

static void macFnx(UArg arg0, UArg arg1)
{
    Semaphore_Params semParam;

    Semaphore_Params_init(&semParam);
    semParam.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&macSem, 0, &semParam);
    macSemHandle = Semaphore_handle(&macSem);

    // Initialize the EasyLink parameters to their default values
     EasyLink_Params easyLink_params;
     EasyLink_Params_init(&easyLink_params);

     TX_init(macSemHandle);
     RX_init(macSemHandle);
     /*
      * Initialize EasyLink with the settings found in ti_easylink_config.h
      * Modify EASYLINK_PARAM_CONFIG in ti_easylink_config.h to change the default
      * PHY
      */
     if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
     {
         System_abort("EasyLink_init failed");
     }
     CLI_startREAD();
     Node_init(macSemHandle);


    while (1)
    {
        //MAC_TASK_TX_DONE_EVT
        if (macEvents & MAC_TASK_CLI_UPDATE_EVT)
        {
            CLI_processCliUpdate();
            Util_clearEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);
        }

        if (macEvents & MAC_TASK_TX_DONE_EVT)
        {
            processTxDone();
            Util_clearEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_RX_DONE_EVT)
        {
            processRxDone();
            Util_clearEvent(&macEvents, MAC_TASK_RX_DONE_EVT);
        }

        if (macEvents & MAC_TASK_NODE_TIMEOUT_EVT)
        {
            CLI_cliPrintf("\r\nTimeout test");
            Util_clearEvent(&macEvents, MAC_TASK_NODE_TIMEOUT_EVT);
        }

        if (macEvents == 0)
        {
            Semaphore_pend(macSemHandle, BIOS_WAIT_FOREVER );
        }

    }
}

static void stateMachineAckContent(Mac_smStateCodes_t argStateCode)
{
    SmFunc stateFn;
    Mac_smStateCodes_t stateCode = argStateCode;
    while (1)
    {
        stateFn = gSmFpArr[stateCode];

    }
}


void Mac_cliUpdate()
{

    Util_setEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSemHandle);

}

static void processTxDone()
{

}

static void processRxDone()
{
    MAC_crsPacket_t pkt;
    RX_getPacket(&pkt);
}



