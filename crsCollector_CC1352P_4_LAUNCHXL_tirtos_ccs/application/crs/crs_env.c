/*
 * crs_env.c
 *
 *  Created on: 12 Jun 2022
 *      Author: tomer_zur
 */

/******************************************************************************
 Includes
 *****************************************************************************/

#include <ti/drivers/NVS.h>
#include <string.h>
#include "crs_vars.h"
#include "crs_nvs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define STRLEN_BYTES 32
#define MAX_LINE_CHARS 1024

#define ENV_FILENAME "env"
#ifndef CLI_SENSOR
    #define ENV_FILE "name=Cdu\nver=0\nconfig=0\nimg=0"
#else
    #define ENV_FILE "name=Cru\nver=0\nconfig=0\nimg=0"
#endif

/******************************************************************************
 Local variables
 *****************************************************************************/

static char * envCache  = NULL;
static NVS_Handle envHandle;


/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Env_restore()
{

    if(envCache == NULL){
        return CRS_FAILURE;
        //Env_init();
    }

    if (Nvs_isFileExists(ENV_FILENAME) == CRS_SUCCESS)
    {
        CRS_free(envCache);
        envCache = Nvs_readFileWithMalloc(ENV_FILENAME);
        if (!envCache)
        {
            envCache = CRS_calloc(1, sizeof(char));
        }
    }
    else
    {
        envCache = CRS_realloc(envCache, sizeof(ENV_FILE));
        memcpy(envCache, ENV_FILE, sizeof(ENV_FILE));

    }
   return Vars_setFile(&envHandle, envCache);

}

CRS_retVal_t Env_init(){

    if (envHandle == NULL)
    {
        NVS_Params nvsParams;
        NVS_init();
        NVS_Params_init(&nvsParams);
        envHandle = NVS_open(ENV_NVS, &nvsParams);

        if (envHandle == NULL)
        {
            // CLI_cliPrintf("NVS_open() failed.\r\n");
            return CRS_FAILURE;
        }
    }

    CRS_retVal_t status;
    int length = Vars_getLength(&envHandle);
    if(length == -1){
        bool ret = Vars_createFile(&envHandle);
        if(!ret){
            return CRS_FAILURE;
        }
        length = 1;
        envCache = CRS_calloc(length, sizeof(char));
        status = Env_restore();
    }else{
        envCache = CRS_calloc(length, sizeof(char));
        status = Vars_getFile(&envHandle, envCache);
    }

    return status;
}

CRS_retVal_t Env_read(char *vars, char *returnedVars){

    if(envCache == NULL){
        Env_init();
    }
    if(strlen(vars)){
        Vars_getVars(envCache, vars, returnedVars);
    }
    else{
        if(strlen(envCache)){
            strcpy(returnedVars, envCache);
            if(returnedVars[strlen(envCache)-1] == '\n'){
                returnedVars[strlen(envCache)-1] = 0;
            }
        }
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Env_write(char *vars){

    uint32_t length;
    if(envCache == NULL){
        Env_init();
    }

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-STRLEN_BYTES);
    length = Vars_setFileVars(&envHandle, envCache, vars);

    envCache = CRS_realloc(envCache, length);

    return CRS_SUCCESS;
}

CRS_retVal_t Env_delete(char *vars){

    uint32_t length;
    if(envCache == NULL){
        Env_init();
    }

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-STRLEN_BYTES);
    length = Vars_removeFileVars(&envHandle, envCache, vars);

    envCache = CRS_realloc(envCache, length);

    return CRS_SUCCESS;
}

CRS_retVal_t Env_format(){

    if (envHandle == NULL)
    {
        NVS_Params nvsParams;
        NVS_init();
        NVS_Params_init(&nvsParams);
        envHandle = NVS_open(ENV_NVS, &nvsParams);
    }

    bool ret = Vars_createFile(&envHandle);
    CRS_free(envCache);
    envCache = CRS_calloc(1, sizeof(char));
    return CRS_SUCCESS;
}

