/*
 * crs_env.c
 *
 *  Created on: 12 Jun 2022
 *      Author: tomer_zur
 */
#include <ti/drivers/NVS.h>
#include <string.h>
#include "crs_vars.h"

static char * envCache  = NULL;
static NVS_Handle envHandle;
static void getEnv();

static CRS_retVal_t Env_init(){
    NVS_Params nvsParams;
    NVS_init();
    NVS_Params_init(&nvsParams);
    envHandle = NVS_open(ENV_NVS, &nvsParams);

    if (envHandle == NULL)
    {
        // CLI_cliPrintf("NVS_open() failed.\r\n");

        return CRS_FAILURE;
    }
    uint32_t length = Vars_getLength(&envHandle);
    envCache = CRS_calloc(length, sizeof(char));
    Vars_getFile(&envHandle, envCache);

    return CRS_SUCCESS;
}

static void getEnv(){
    uint32_t length = Vars_getLength(&envHandle);
    envCache = CRS_calloc(length, sizeof(char));
    Vars_getFile(&envHandle, envCache);

}

CRS_retVal_t Env_read(char *vars, char *returnedVars){

    if(envCache == NULL){
        Env_init();
    }

    Vars_getVars(envCache, vars, returnedVars);
    return CRS_SUCCESS;
}

CRS_retVal_t Env_write(char *vars){

    uint32_t length;
    if(envCache == NULL){
        Env_init();
    }

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize);
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

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize);
    length = Vars_removeFromFile(&envHandle, envCache, vars);

    envCache = CRS_realloc(envCache, length);

    return CRS_SUCCESS;
}


