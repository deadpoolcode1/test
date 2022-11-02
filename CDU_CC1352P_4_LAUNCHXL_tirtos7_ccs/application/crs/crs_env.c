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
static NVS_Handle envHandle;
static NVS_Attrs gRegionAttrs;
static bool gIsMoudleInit = false;


/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Env_restore()
{
    if (nvsInit() != CRS_SUCCESS)
        {
            return CRS_FAILURE;
        }
    if(envCache == NULL){
        nvsClose();

        return CRS_FAILURE;
        //Env_init();
    }

    if (Nvs_isFileExists(ENV_FILENAME) == CRS_SUCCESS)
    {
        CRS_free(envCache);
//        envCache = Nvs_readFileWithMalloc(ENV_FILENAME);
//        if (!envCache)
//        {
//            envCache = CRS_calloc(1, sizeof(char));
//        }
        envCache = CRS_realloc(envCache, sizeof(ENV_FILE));
        memcpy(envCache, ENV_FILE, sizeof(ENV_FILE));
    }
    else
    {
        envCache = CRS_realloc(envCache, sizeof(ENV_FILE));
        memcpy(envCache, ENV_FILE, sizeof(ENV_FILE));

    }
    CRS_retVal_t retStatus = Vars_setFile(&envHandle, envCache);

    nvsClose();

   return retStatus;

}

CRS_retVal_t Env_init(){

    if (nvsInit() != CRS_SUCCESS)
        {
            return CRS_FAILURE;
        }
    /*
         * This will populate a NVS_Attrs structure with properties specific
         * to a NVS_Handle such as region base address, region size,
         * and sector size.
         */
    NVS_getAttrs(envHandle, &gRegionAttrs);

    CRS_retVal_t status;
    int length = Vars_getLength(&envHandle);
    if(length == -1){
        bool ret = Vars_createFile(&envHandle);
        if(!ret){
            nvsClose();

            return CRS_FAILURE;
        }
        length = 1;
        envCache = CRS_calloc(length, sizeof(char));
        status = Env_restore();
    }else{
        envCache = CRS_calloc(sizeof(ENV_FILE), sizeof(char));
//        status = Vars_getFile(&envHandle, envCache); // TODO fix this
        memcpy(envCache,ENV_FILE, sizeof(ENV_FILE));
    }
    nvsClose();

    return status;
}

CRS_retVal_t Env_read(char *vars, char *returnedVars){

    if (nvsInit() != CRS_SUCCESS)
    {
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
            strcpy(returnedVars, envCache);
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
        return CRS_FAILURE;
    }

    uint32_t length;

    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-STRLEN_BYTES);
    length = Vars_setFileVars(&envHandle, envCache, vars);

    envCache = CRS_realloc(envCache, length);
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Env_delete(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    uint32_t length;


    NVS_Attrs envRegionAttrs;
    NVS_getAttrs(envHandle, &envRegionAttrs);

    envCache = CRS_realloc(envCache, envRegionAttrs.regionSize-STRLEN_BYTES);
    length = Vars_removeFileVars(&envHandle, envCache, vars);
    if(!length){
        nvsClose();

        return CRS_FAILURE;
    }
    envCache = CRS_realloc(envCache, length);
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Env_format(){
    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }
//    int_fast16_t retStatus = NVS_erase(envHandle, 0, gRegionAttrs.regionSize);
    bool ret = Vars_createFile(&envHandle);
    CRS_free(envCache);
    envCache = CRS_calloc(1, sizeof(char));
    nvsClose();

    return CRS_SUCCESS;
}


static CRS_retVal_t nvsInit()
{
    if (gIsMoudleInit == true)
    {
        return CRS_SUCCESS;
    }
    NVS_Params nvsParams;
    NVS_Params_init(&nvsParams);

    envHandle = NVS_open(ENV_NVS, &nvsParams);

    if (envHandle == NULL)
    {

        return (CRS_FAILURE);
    }



    gIsMoudleInit = true;
    return CRS_SUCCESS;
}

static CRS_retVal_t nvsClose()
{
    gIsMoudleInit = false;

    if (envHandle == NULL)
    {
        return CRS_SUCCESS;
    }
    NVS_close(envHandle);
    envHandle = NULL;
    return CRS_SUCCESS;
}

