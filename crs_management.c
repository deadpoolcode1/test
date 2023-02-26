/*
 * crs_management.c
 *
 *  Created on: 16 May 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/

#include "application/crs/crs_management.h"
#include "mac/mac_util.h"
#include <ti/sysbios/BIOS.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define RUN_NEXT_TASK_EVT 0x0001

/******************************************************************************
 Local variables
 *****************************************************************************/
static Mailbox_Params mbxParams;
static Mailbox_Handle mbxHandle;

static Semaphore_Handle collectorSem;

static uint16_t Manage_events = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static CRS_retVal_t runTask(Manage_taskObj_t *task);
static CRS_retVal_t getTask(Manage_taskObj_t *task);

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Manage_initSem(void *sem)
{
    collectorSem = sem;
    Mailbox_Params_init(&mbxParams);
    mbxHandle = Mailbox_create(sizeof(Manage_taskObj_t), 10, &mbxParams,
                               Error_IGNORE);

}

void Manage_process()
{
    if (Manage_events & RUN_NEXT_TASK_EVT)
    {
        Manage_taskObj_t task = { 0 };
        CRS_retVal_t rsp = getTask(&task);
        if (rsp == CRS_SUCCESS)
        {
            runTask(&task);
        }
        /* Clear the event */
        Util_clearEvent(&Manage_events, RUN_NEXT_TASK_EVT);
    }
}

static CRS_retVal_t runTask(Manage_taskObj_t *task)
{
    task->cbFn(NULL);
    return CRS_SUCCESS;
}

CRS_retVal_t Manage_addTask(Manage__cbFn_t _cbFn, Manage__cbArgs_t *_cbArgs)
{
    Manage_taskObj_t task = { 0 };
    task.cbFn = _cbFn;
    Mailbox_post(mbxHandle, &task, BIOS_NO_WAIT);
    if (Mailbox_getNumPendingMsgs(mbxHandle) == 1)
    {
        //set event
        Util_setEvent(&Manage_events, RUN_NEXT_TASK_EVT);

        /* Wake up the application thread when it waits for clock event */
        Semaphore_post(collectorSem);
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Manage_finishedTask()
{
    //set event
    Util_setEvent(&Manage_events, RUN_NEXT_TASK_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
    return CRS_SUCCESS;
}

static CRS_retVal_t getTask(Manage_taskObj_t *task)
{

    bool rsp = Mailbox_pend(mbxHandle, task, BIOS_NO_WAIT);
    if (rsp == true)
    {
        return CRS_SUCCESS;
    }
    else
    {
        return CRS_FAILURE;
    }

}

