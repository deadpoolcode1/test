/*
 * crs_vars.h
 *
 *  Created on: 12 Jun 2022
 *      Author: epc_5
 */

#ifndef APPLICATION_CRS_CRS_VARS_H_
#define APPLICATION_CRS_CRS_VARS_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/drivers/NVS.h>
#include "crs.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
//CRS_retVal_t Vars_init();
//CRS_retVal_t Vars_readVarsFile(char *vars, char *returnedVars, NVS_Handle fileHandle, char * fileCache);
//CRS_retVal_t Vars_setVarsFile(char *vars, NVS_Handle fileHandle, char * fileCache);
//CRS_retVal_t Vars_rmVarsFile(char *vars, NVS_Handle fileHandle, char * fileCache);

//CRS_retVal_t Vars_format(NVS_Handle fileHandle, char * file);
//CRS_retVal_t Vars_restore(NVS_Handle fileHandle, char * file);

int Vars_getLength(NVS_Handle * fileHandle);
bool Vars_createFile(NVS_Handle * fileHandle);
CRS_retVal_t Vars_getFile(NVS_Handle * fileHandle, char * file);
uint16_t Vars_setFileVars(NVS_Handle * fileHandle, char * file, char * vars);
uint16_t Vars_removeFileVars(NVS_Handle * fileHandle, char * file, char * vars);
void Vars_getVars(char *file, char *keys, char *values);
void Vars_setVars(char *file, char *vars, const char *d);
void Vars_removeVars(char *file, char *keys);


#endif /* APPLICATION_CRS_CRS_VARS_H_ */
