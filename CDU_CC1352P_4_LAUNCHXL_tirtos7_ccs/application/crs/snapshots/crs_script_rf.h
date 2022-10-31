/*
 * crs_script_rf.h
 *
 *  Created on: Oct 24, 2022
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RF_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RF_H_



/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"

#define SCRIPT_RF_NAME_VALUES_SZ 10


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t scriptRf_init(void);
CRS_retVal_t scriptRf_runFile(uint8_t *filename, CRS_nameValue_t nameVals[SCRIPT_RF_NAME_VALUES_SZ], uint32_t chipNumber, uint32_t lineNumber);


#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RF_H_ */
