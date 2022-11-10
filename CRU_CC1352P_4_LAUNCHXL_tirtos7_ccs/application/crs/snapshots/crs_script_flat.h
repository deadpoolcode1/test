/*
 * crs_script_flat.h
 *
 *  Created on: Nov 3, 2022
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_FLAT_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_FLAT_H_


/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"

#define SCRIPT_FLAT_NAME_VALUES_SZ 10


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t scriptFlat_init(void);
CRS_retVal_t scriptFlat_runFile(uint8_t *filename);



#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_FLAT_H_ */
