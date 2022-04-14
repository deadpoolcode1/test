/*
 * sm_discovery.h
 *
 *  Created on: 14 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_SM_DISCOVERY_H_
#define MAC_SM_DISCOVERY_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "macTask.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void Smd_init(void *sem);
void Smd_process();
void Smd_startDiscovery();
void Smd_printStateMachine();

#endif /* MAC_SM_DISCOVERY_H_ */
