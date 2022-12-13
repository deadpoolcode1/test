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
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t nvsInit();
static CRS_retVal_t nvsClose();

/******************************************************************************
 Local variables
 *****************************************************************************/

static char * envCache  = NULL;
static NVS_Handle envHandle = NULL;
static bool gIsMoudleInit = false;


/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Env_init(void){

    if (nvsInit() != CRS_SUCCESS)
        {
            CRS_LOG(CRS_ERR, "\r\nnvsInit failed for EnvInit");
            return CRS_FAILURE;
        }
    /*
         * This will populate a NVS_Attrs structure with properties specific
         * to a NVS_Handle such as region base address, region size,
         * and sector size.
         */

    CRS_retVal_t status=CRS_FAILURE;
    int length = Vars_getLength(&envHandle);
    if(length == -1){ //there is no hdr found in flash
        bool ret = Vars_createFile(&envHandle);
        if(!ret){
            nvsClose();

            return CRS_FAILURE;
        }
        length = 1;
        envCache = CRS_calloc(length, sizeof(char));
        if (NULL == envCache){
            CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");
            nvsClose();
            return CRS_FAILURE;
        }
        memset(envCache, '\0', length);
        status = Env_restore();
    }else{
        envCache = CRS_calloc(length, sizeof(char));
        if (NULL == envCache){
            CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");
            nvsClose();
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
    nvsClose();

    return status;
}

CRS_retVal_t Env_read(char *vars, char *returnedVars){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Env_read");
        return CRS_FAILURE;
    }

    if(strlen(vars)){
        bool exists = Vars_getVars(envCache, vars, returnedVars);
        if(!exists){
            nvsClose();

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
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Env_write(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Env_write");
        return CRS_FAILURE;
    }

    uint32_t length = 0;

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-VARS_HDR_SZ_BYTES); //TODO check why this is indeed
    if (envCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nenvCache failed to realloc!");
        nvsClose();
        return CRS_FAILURE;
    }
    length = Vars_setFileVars(&envHandle, envCache, vars);
    if (length == 0){
        CRS_LOG(CRS_ERR,"\r\nVars_setFileVars failed");
        nvsClose();

        return CRS_FAILURE;
    }

    envCache = CRS_realloc(envCache, length);
    if (envCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nenvCache failed to realloc!");
        nvsClose();
        return CRS_FAILURE;
    }

    nvsClose();
    return CRS_SUCCESS;
}

CRS_retVal_t Env_delete(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Env_delete");
        return CRS_FAILURE;
    }
    uint32_t length = 0;


    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-VARS_HDR_SZ_BYTES); // TODO WHY is this needed?
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }

    length = Vars_removeFileVars(&envHandle, envCache, vars);
    if(0 == length){
        CRS_LOG(CRS_ERR, "\r\nVars_removeFileVars failed!");

        nvsClose();

        return CRS_FAILURE;
    }

    envCache = CRS_realloc(envCache, length);
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Env_format(void){
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Env_delete");
        return CRS_FAILURE;
    }
    bool ret = Vars_createFile(&envHandle);
    if (ret == false){
        CRS_LOG(CRS_ERR, "\r\nVars_createFile failed!");
        nvsClose();
        return CRS_FAILURE;
    }
    CRS_free(&envCache);
    envCache = CRS_calloc(1, sizeof(char));
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache calloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Env_restore()
{
    if (nvsInit() != CRS_SUCCESS)
        {
            CRS_LOG(CRS_ERR, "\r\nnvsInit failed for EnvInit");
            return CRS_FAILURE;
        }

    if(envCache == NULL){
        nvsClose();

        return CRS_FAILURE;
        //Env_init();
    }

    CRS_free(&envCache);
    envCache = CRS_realloc(envCache, sizeof(ENV_FILE)); // one more on purpose for end byte
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }

    memset(envCache, '\0', sizeof(ENV_FILE));

    memcpy(envCache, ENV_FILE, sizeof(ENV_FILE) - 1); //write into RAM cache
    CRS_retVal_t ret =  Vars_setFile(&envHandle, envCache); //write into flash
    nvsClose();
    if (ret != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nVars_setFile failed");
    }

    return ret;
}

