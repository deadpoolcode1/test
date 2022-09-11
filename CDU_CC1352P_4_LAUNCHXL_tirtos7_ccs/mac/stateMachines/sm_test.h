/*
 * sm_test.h
 *
 *  Created on: 5 Sep 2022
 *      Author: epc_4
 */

#ifndef MAC_STATEMACHINES_SM_TEST_H_
#define MAC_STATEMACHINES_SM_TEST_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "mac/macTask.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void Smt_init(void *sem);
void Smt_process();
void Smd_startTest(uint32_t time, uint8_t msduHandle);

#endif /* MAC_STATEMACHINES_SM_TEST_H_ */
