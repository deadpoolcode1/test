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
#include "crs_env.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define VARS_HDR_SZ_BYTES 32
#define MAX_LINE_CHARS 1024

#define ENV_FILENAME "env"
#ifndef CLI_SENSOR
    #define ENV_FILE "name=Cdu\nver=0\nconfig=0\nimg=0\nledMode=1"
#else
    #define ENV_FILE "name=Cru\nver=0\nconfig=0\nimg=0\nledMode=1"
#endif

/******************************************************************************
 Local variables
 *****************************************************************************/

static char * envCache  = NULL;
static NVS_Handle envHandle = NULL;
static NVS_Attrs gRegionAttrs = {0};

/******************************************************************************
 Public Functions
 *****************************************************************************/



CRS_retVal_t Env_init(void){

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
    /*
         * This will populate a NVS_Attrs structure with properties specific
         * to a NVS_Handle such as region base address, region size,
         * and sector size.
         */
    NVS_getAttrs(envHandle, &gRegionAttrs);

    CRS_retVal_t status=CRS_FAILURE;
    int length = Vars_getLength(&envHandle);
    if(length == -1){ //there is no hdr found in flash
        bool ret = Vars_createFile(&envHandle);
        if(!ret){
            return CRS_FAILURE;
        }
        length = 1;
        envCache = CRS_calloc(length, sizeof(char));
        if (NULL == envCache){
            CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");

            return CRS_FAILURE;
        }
        memset(envCache, '\0', length);
        status = Env_restore();
    }else{
        envCache = CRS_calloc(length, sizeof(char));
        if (NULL == envCache){
            CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");

            return CRS_FAILURE;
        }
        memset(envCache, '\0', length);
#ifndef CLI_SENSOR
          Env_write("name=Cdu");
#else
          Env_write("name=Cru");
#endif
        status = Vars_getFile(&envHandle, envCache);
    }

    return status;
}

CRS_retVal_t Env_read(char *vars, char *returnedVars){

    if(envCache == NULL){
        Env_init();
    }
    if(strlen(vars)){
        bool exists = Vars_getVars(envCache, vars, returnedVars);
        if(!exists){
            return CRS_FAILURE;
        }
    }
    else{
        if(strlen(envCache)){
            memcpy(returnedVars, envCache,strlen(envCache));
            if(returnedVars[strlen(envCache)-1] == '\n'){
                returnedVars[strlen(envCache)-1] = 0;
            }
        }
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Env_write(char *vars){

    uint16_t length=0;
    if(envCache == NULL){
        CRS_retVal_t ret=  Env_init();
        if (ret==CRS_FAILURE) {
            CRS_LOG(CRS_ERR,"\r\nEnv init failed!");

            return ret;
        }
    }

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-VARS_HDR_SZ_BYTES); //TODO check why this is indeed
    if (envCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nenvCache failed to realloc!");
        return CRS_FAILURE;
    }
    length = Vars_setFileVars(&envHandle, envCache, vars);
    if (length == 0){
        CRS_LOG(CRS_ERR,"\r\nVars_setFileVars failed");

        return CRS_FAILURE;
    }

    envCache = CRS_realloc(envCache, length);
    if (envCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nenvCache failed to realloc!");

        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}

CRS_retVal_t Env_delete(char *vars){

    uint32_t length;
    if(envCache == NULL){
        Env_init();
    }

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-VARS_HDR_SZ_BYTES); // TODO WHY is this needed?
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");

        return CRS_FAILURE;
    }

    length = Vars_removeFileVars(&envHandle, envCache, vars);
    if(0 == length){
        CRS_LOG(CRS_ERR, "\r\nVars_removeFileVars failed!");

        return CRS_FAILURE;
    }

    envCache = CRS_realloc(envCache, length);
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");

        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}

CRS_retVal_t Env_format(void){
    if (envHandle == NULL)
    {
        NVS_Params nvsParams;
        NVS_init();
        NVS_Params_init(&nvsParams);
        envHandle = NVS_open(ENV_NVS, &nvsParams);
        if (NULL == envHandle){
            CRS_LOG(CRS_ERR, "\r\nNVS_open failed!");

            return CRS_FAILURE;
        }
    }
//    int_fast16_t retStatus = NVS_erase(envHandle, 0, gRegionAttrs.regionSize);
    bool ret = Vars_createFile(&envHandle);
    if (ret == false){
        CRS_LOG(CRS_ERR, "\r\nVars_createFile failed!");

        return CRS_FAILURE;
    }
    CRS_free(&envCache);
    envCache = CRS_calloc(1, sizeof(char));
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");

        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Env_restore(void)
{

    if(envCache == NULL){
        CRS_LOG(CRS_ERR,"\r\nenvCache was NULL when calling Env_restore");

        return CRS_FAILURE;
    }

    CRS_free(&envCache);
    envCache = CRS_realloc(envCache, sizeof(ENV_FILE)); // one more on purpose for end byte
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");

        return CRS_FAILURE;
    }

    memset(envCache, '\0', sizeof(ENV_FILE));

    memcpy(envCache, ENV_FILE, sizeof(ENV_FILE) - 1); //write into RAM cache
    return Vars_setFile(&envHandle, envCache); //write into flash

}

