/*
 * crs_thresholds.c
 *
 *  Created on: 17 Mar 2022
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
#define THRSH_FILENAME "thrsh"
#ifndef CLI_SENSOR
#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\n\
MaxCableLossThr=35\nDLMaxInputPower=-35\nDLRxMaxGainThr=45\nULTxMaxGainThr=55\nULMaxOutputPower=20\n\
SyncMode=Manual\nCPType=Normal\nSensorMode=0\nDLRxGain=28\nULTxGain=28\nCblCompFctr=22\nModemTxPwr=13\n\
UpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\nAgcInterval=1000"
#else
#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\n\
MaxCableLossThr=35\nULMaxInputPower=-35\nULRxMaxGainThr=60\nDLTxMaxGainThr=55\nDLMaxOutputPower=20\n\
DLSystemMaxGainThr=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nULRxGain=30\nDLTxGain=28\n\
DLSystemGain=20\nCblCompFctr=9\nModemTxPwr=13\nUpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\n\
AgcInterval=1000"
#endif

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t nvsInit();
static CRS_retVal_t nvsClose();

/******************************************************************************
 Local variables
 *****************************************************************************/

static char * threshCache  = NULL;
static NVS_Handle threshHandle;
static bool gIsMoudleInit = false;

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Thresh_restore()
{
    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

    if(threshCache == NULL){
        nvsClose();

        return CRS_FAILURE;
        //Thresh_init();
    }

    if (Nvs_isFileExists(THRSH_FILENAME) == CRS_SUCCESS)
    {
        CRS_free(threshCache);
        threshCache = Nvs_readFileWithMalloc(THRSH_FILENAME);
        if (!threshCache)
        {
            threshCache = CRS_calloc(1, sizeof(char));
        }
    }
    else
    {
        threshCache = CRS_realloc(threshCache, sizeof(THRSH_FILE));
        memcpy(threshCache, THRSH_FILE, sizeof(THRSH_FILE));

    }
    CRS_retVal_t retStatus = Vars_setFile(&threshHandle, threshCache);
   nvsClose();
   return retStatus;


}

CRS_retVal_t Thresh_init(){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }


    CRS_retVal_t status;
    int length = Vars_getLength(&threshHandle);
    if(length == -1){
        bool ret = Vars_createFile(&threshHandle);
        if(!ret){
            nvsClose();

            return CRS_FAILURE;
        }
        length = 1;
        threshCache = CRS_calloc(length, sizeof(char));
        status = Thresh_restore();
    }else{
        threshCache = CRS_calloc(length, sizeof(char));
        status = Vars_getFile(&threshHandle, threshCache);
    }
    nvsClose();

    return status;
}

CRS_retVal_t Thresh_read(char *vars, char *returnedVars){


    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

    if(strlen(vars)){
        bool exists = Vars_getVars(threshCache, vars, returnedVars);
        if(!exists){
            nvsClose();

            return CRS_FAILURE;
        }
    }
    else{
        if(strlen(threshCache)){
            strcpy(returnedVars, threshCache);
            if(returnedVars[strlen(threshCache)-1] == '\n'){
                returnedVars[strlen(threshCache)-1] = 0;
            }
        }
    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_write(char *vars){


    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

    uint32_t length;


    NVS_Attrs threshRegionAttrs;
    NVS_getAttrs(threshHandle, &threshRegionAttrs);

    threshCache = CRS_realloc(threshCache, threshRegionAttrs.regionSize);
    length = Vars_setFileVars(&threshHandle, threshCache, vars);

    threshCache = CRS_realloc(threshCache, length);
    nvsClose();

//    char filecopy [1024] = {0};
//    NVS_read(threshHandle, 0, (void*) filecopy, 1024);
    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_delete(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    uint32_t length;


    NVS_Attrs threshRegionAttrs;
    NVS_getAttrs(threshHandle, &threshRegionAttrs);

    threshCache = CRS_realloc(threshCache, threshRegionAttrs.regionSize);
    length = Vars_removeFileVars(&threshHandle, threshCache, vars);
    if(!length){
        nvsClose();

        return CRS_FAILURE;
    }
    threshCache = CRS_realloc(threshCache, length);
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_format(){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }


    bool ret = Vars_createFile(&threshHandle);
    CRS_free(threshCache);
    threshCache = CRS_calloc(1, sizeof(char));
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

    threshHandle = NVS_open(THRESH_NVS, &nvsParams);

    if (threshHandle == NULL)
    {

        return (CRS_FAILURE);
    }

    gIsMoudleInit = true;
    return CRS_SUCCESS;
}

static CRS_retVal_t nvsClose()
{
    gIsMoudleInit = false;

    if (threshHandle == NULL)
    {
        return CRS_SUCCESS;
    }
    NVS_close(threshHandle);
    threshHandle = NULL;
    return CRS_SUCCESS;
}

