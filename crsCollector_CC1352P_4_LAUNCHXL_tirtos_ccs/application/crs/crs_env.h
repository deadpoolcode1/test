/*
 * crs_env.h
 *
 *  Created on: 12 Jun 2022
 *      Author: epc_5
 */

#ifndef APPLICATION_CRS_CRS_ENV_H_
#define APPLICATION_CRS_CRS_ENV_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <ti/drivers/NVS.h>
#include "crs.h"

CRS_retVal_t Env_read(char *vars, char *returnedVars);
CRS_retVal_t Env_write(char *vars);
CRS_retVal_t Env_delete(char *vars);
CRS_retVal_t Env_format();

#endif /* APPLICATION_CRS_CRS_ENV_H_ */
