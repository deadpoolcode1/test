/*
 * crs_agc_management.c
 *
 *  Created on: 18 Jul 2022
 *      Author: epc_4
 */


/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_agc_management.h"


/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define TASK_ARR_SIZE 10

/******************************************************************************
 Local variables
 *****************************************************************************/

//static void *sem;
static AGCM_cbFn_t gTaskArr[TASK_ARR_SIZE] = {0};
static uint32_t gTaskArrIdx = 0;
static volatile bool gIsTaskRunnig = false;

/******************************************************************************
 Local function prototypes
 *****************************************************************************/

static bool isThereNextTask();
static bool addTask(AGCM_cbFn_t cb);
//static bool runTask(AGCM_cbFn_t cb);
static bool runNextTask();


/******************************************************************************
 Public Functions
 *****************************************************************************/


bool AGCM_runTask(AGCM_cbFn_t cb)
{
    if (gIsTaskRunnig == true)
    {
        return addTask(cb);
    }
    else
    {
        gIsTaskRunnig = true;
        cb();
        return true;
    }

}

bool AGCM_finishedTask()
{
    bool res = isThereNextTask();
    if (res == false)
    {
        gIsTaskRunnig = false;

        return true;
    }

    return runNextTask();
}

/******************************************************************************
 Local Functions
 *****************************************************************************/

static bool isThereNextTask()
{
    if (gTaskArrIdx == 0)
    {
        return false;
    }
    return true;
}

static bool addTask(AGCM_cbFn_t cb)
{
    if (cb == NULL)
    {
        return false;
    }

    if (gTaskArrIdx >= TASK_ARR_SIZE)
    {
        return false;
    }

    gTaskArr[gTaskArrIdx] = cb;
    gTaskArrIdx++;
    return true;

}

//static bool runTask(AGCM_cbFn_t cb)
//{
//
//}

static bool runNextTask()
{
    if (isThereNextTask() == false)
    {
        return false;
    }

    AGCM_cbFn_t cb = gTaskArr[gTaskArrIdx - 1];
    gTaskArr[gTaskArrIdx - 1] = NULL;

    gTaskArrIdx--;

    if (cb != NULL)
    {
        cb();
    }
    return true;
}



