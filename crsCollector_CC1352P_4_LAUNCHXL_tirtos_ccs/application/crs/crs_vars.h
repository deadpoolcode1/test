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

int Vars_getLength(NVS_Handle * fileHandle);
bool Vars_createFile(NVS_Handle * fileHandle);
CRS_retVal_t Vars_getFile(NVS_Handle * fileHandle, char * file);
CRS_retVal_t Vars_setFile(NVS_Handle * fileHandle, char * file);
uint16_t Vars_setFileVars(NVS_Handle * fileHandle, char * file, char * vars);
uint16_t Vars_removeFileVars(NVS_Handle * fileHandle, char * file, char * vars);
void Vars_getVars(char *file, char *keys, char *values);
void Vars_setVars(char *file, char *vars, const char *d, char * returnedFile);
void Vars_removeVars(char *file, char *keys, char * returnedFile);
CRS_retVal_t Vars_format(NVS_Handle *fileHandle);

#endif /* APPLICATION_CRS_CRS_VARS_H_ */
