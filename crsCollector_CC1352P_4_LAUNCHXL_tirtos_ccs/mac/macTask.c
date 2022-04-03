/*
 * macTask.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "macTask.h"
#include "cp_cli.h"

#define MAC_TASK_STACK_SIZE    1024
#define MAC_TASK_PRIORITY      2

#define MAC_TASK_CLI_UPDATE_EVT 0x00000001

Task_Struct macTask; /* not static so you can see in ROV */
static Task_Params macTaskParams;
static uint8_t macTaskStack[MAC_TASK_STACK_SIZE];

Semaphore_Struct macSem; /* not static so you can see in ROV */
static Semaphore_Handle macSemHandle;

/*! Storage for Events flags */
uint32_t macEvents = 0;

static void setEvents(uint32_t *eventVar, uint32_t *eventType);
static void clearEvents(uint32_t *eventVar, uint32_t *eventType);

static void macFnx(UArg arg0, UArg arg1);

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

    while (1)
    {
        if (macEvents & MAC_TASK_CLI_UPDATE_EVT)
        {
            CLI_processCliUpdate();
            clearEvents(&macEvents, MAC_TASK_CLI_UPDATE_EVT);
        }

        Semaphore_pend(macSemHandle, BIOS_WAIT_FOREVER );

    }
}

void Mac_cliUpdate()
{

    setEvent(&macEvents, MAC_TASK_CLI_UPDATE_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(macSemHandle);

}

static void setEvents(uint32_t *eventVar, uint32_t *eventType)
{

}

static void clearEvents(uint32_t *eventVar, uint32_t *eventType)
{

}

