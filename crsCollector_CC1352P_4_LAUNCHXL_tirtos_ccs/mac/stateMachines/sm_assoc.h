/*
 * sm_assoc.h
 *
 *  Created on: 18 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_STATEMACHINES_SM_ASSOC_H_
#define MAC_STATEMACHINES_SM_ASSOC_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "macTask.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void Smas_init(void *sem);
void Smas_process();



#endif /* MAC_STATEMACHINES_SM_ASSOC_H_ */
