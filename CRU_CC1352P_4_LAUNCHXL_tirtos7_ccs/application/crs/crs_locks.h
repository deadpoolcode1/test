/*
 * crs_locks.h
 *
 *  Created on: Oct 30, 2022
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_CRS_LOCKS_H_
#define APPLICATION_CRS_CRS_LOCKS_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t Locks_init(void *sem);
CRS_retVal_t Locks_process(void);
void Locks_checkLocks(void);
#ifndef CLI_SENSOR
bool Locks_getTddLockVal(void);
#else
bool Locks_getAdfLockVal(void);
#endif
bool Locks_getTiLockVal(void);

#endif /* APPLICATION_CRS_CRS_LOCKS_H_ */
