/*
 * crs_thresholds.h
 *
 *  Created on: 17 Mar 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_THRESHOLDS_H_
#define APPLICATION_CRS_CRS_THRESHOLDS_H_
/******************************************************************************
 Includes
 *****************************************************************************/

#include <ti/drivers/NVS.h>
#include "crs.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

//CRS_retVal_t Thresh_restore(int fileIndex);

CRS_retVal_t Thresh_read(char *vars, char *returnedVars);
CRS_retVal_t Thresh_write(char *vars);
CRS_retVal_t Thresh_delete(char *vars);
CRS_retVal_t Thresh_format();
CRS_retVal_t Thresh_restore();

#endif /* APPLICATION_CRS_CRS_THRESHOLDS_H_ */
