/*
 * sm_rx_idle.h
 *
 *  Created on: 18 Apr 2022
 *      Author: epc_4
 */

#ifndef MAC_STATEMACHINES_SM_RX_IDLE_H_
#define MAC_STATEMACHINES_SM_RX_IDLE_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "macTask.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void Smri_init(void *sem);
void Smri_process();


#endif /* MAC_STATEMACHINES_SM_RX_IDLE_H_ */
