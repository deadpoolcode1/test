/*
 * crs_thresholds.h
 *
 *  Created on: 17 Mar 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_THRESHOLDS_H_
#define APPLICATION_CRS_CRS_THRESHOLDS_H_

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "crs.h"


CRS_retVal_t Thresh_init();

CRS_retVal_t Thresh_readVarsFile(char *vars, char *returnedVars, int fileIndex);
CRS_retVal_t Thresh_setVarsFile(char* vars, int fileIndex);
CRS_retVal_t Thresh_rmVarsFile(char* vars, int fileIndex);
CRS_retVal_t Thresh_format(int fileIndex);


#endif /* APPLICATION_CRS_CRS_THRESHOLDS_H_ */
