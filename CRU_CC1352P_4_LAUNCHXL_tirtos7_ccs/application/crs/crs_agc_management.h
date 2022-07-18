/*
 * crs_agc_management.h
 *
 *  Created on: 18 Jul 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_AGC_MANAGEMENT_H_
#define APPLICATION_CRS_CRS_AGC_MANAGEMENT_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "crs.h"
#include "string.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

typedef void (*AGCM_cbFn_t)(void);


/******************************************************************************
 Function Prototypes
 *****************************************************************************/


bool AGCM_runTask(AGCM_cbFn_t cb);
bool AGCM_finishedTask();

#endif /* APPLICATION_CRS_CRS_AGC_MANAGEMENT_H_ */
