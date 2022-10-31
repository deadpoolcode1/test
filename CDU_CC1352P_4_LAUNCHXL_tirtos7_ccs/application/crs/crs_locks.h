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

CRS_retVal_t Lock_init(void *sem);
CRS_retVal_t Lock_process(void);
void Lock_checkLocks(void);


#endif /* APPLICATION_CRS_CRS_LOCKS_H_ */
