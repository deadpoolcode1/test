/*
 * crs_param_validator.h
 *
 *  Created on: Jan 10, 2023
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_PARAM_VALIDATOR_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_PARAM_VALIDATOR_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include "crs.h" // CRS_NAMEVAL_T

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
bool isParamValid(const char *file, const CRS_nameValue_t nameValues[NAME_VALUES_SZ], const char *paramName);


#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_PARAM_VALIDATOR_H_ */
