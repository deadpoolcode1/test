/*
 * crs_script_returnvalues.h
 *
 *  Created on: Jan 11, 2023
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RETURNVALUES_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RETURNVALUES_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include "application/crs/crs.h" // crs_retval_t
/******************************************************************************
 Constants and definitions
 *****************************************************************************/


typedef enum{
    scriptRetVal_OK,
    scriptRetVal_General_failure,
    scriptRetVal_Param_out_of_range,
    scriptRetVal_Invalid_script_line
}scriptReturnValue_t;

#define RETVAL_ELEMENT_VAL_SZ   30
#define RETVAL_ELEMENT_KEY_SZ  15
/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t ScriptRetVals_init(void);
CRS_retVal_t ScriptRetVals_getValue(char *key, char *value);
CRS_retVal_t ScriptRetVals_setValue(char *key, char* value);
CRS_retVal_t ScriptRetVals_setStatus(scriptReturnValue_t status);

#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_RETURNVALUES_H_ */
