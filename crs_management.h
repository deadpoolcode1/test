/*
 * crs_management.h
 *
 *  Created on: 16 May 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_MANAGEMENT_H_
#define APPLICATION_CRS_CRS_MANAGEMENT_H_
/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/



typedef struct {
    uint32_t arg0;
    uint32_t arg1;
    char*    arg3;
} Manage__cbArgs_t;

typedef void (*Manage__cbFn_t)(Manage__cbArgs_t* _cbArgs);

typedef struct
{
    Manage__cbFn_t cbFn;
} Manage_taskObj_t;

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

void Manage_initSem(void *sem);
void Manage_process();
CRS_retVal_t Manage_addTask(Manage__cbFn_t _cbFn, Manage__cbArgs_t* _cbArgs);
CRS_retVal_t Manage_finishedTask();

#endif /* APPLICATION_CRS_CRS_MANAGEMENT_H_ */
