/*
 * crs_yarden.h
 *
 *  Created on: Oct 24, 2022
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_YARDEN_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_YARDEN_H_



/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"

#define YARDEN_NAME_VALUES_SZ 10


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Yarden_init(void);
CRS_retVal_t Yarden_runFile(uint8_t *filename, CRS_nameValue_t nameVals[YARDEN_NAME_VALUES_SZ]);


#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_YARDEN_H_ */
