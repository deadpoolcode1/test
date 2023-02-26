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
#include "crs_thresholds.h"
#include "crs_vars.h"
#include "crs_nvs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define VARS_HDR_SZ_BYTES 32
#define MAX_LINE_CHARS 1024
#define THRSH_FILENAME "thrsh"
#ifndef CLI_SENSOR
//#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\n\
//MaxCableLossThr=35\nDLMaxInputPower=-35\nDLRxMaxGainThr=45\nULTxMaxGainThr=55\nULMaxOutputPower=20\n\
//SyncMode=Manual\nCPType=Normal\nSensorMode=0\nDLRxGain=28\nULTxGain=28\nCblCompFctr=22\nModemTxPwr=13\n\
//UpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\nAgcInterval=1000"

//updated to package 1.5.7
#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\nMaxCableLossThr=35\nDLMaxInputPower=-35\nDLRxMaxGainThr=45\nULTxMaxGainThr=55\nULMaxOutputPower=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nDLRxGain=28\nULTxGain=28\nUpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\nCblCompFctr=22\nModemTxPwr=13"



#else
//#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\n\
//MaxCableLossThr=35\nULMaxInputPower=-35\nULRxMaxGainThr=60\nDLTxMaxGainThr=55\nDLMaxOutputPower=20\n\
//DLSystemMaxGainThr=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nULRxGain=30\nDLTxGain=28\n\
//DLSystemGain=20\nCblCompFctr=9\nModemTxPwr=13\nUpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\n\
//AgcInterval=1000"

//updated to package 1.5.7
#define THRSH_FILE "Technology=5G\nBand=n78\nBandwidth=20\nEARFCN=650000\nCentralFreq=3750\nMIMO=2x2\nMaxCableLossThr=35\nULMaxInputPower=-35\nULRxMaxGainThr=60\nDLTxMaxGainThr=55\nDLMaxOutputPower=20\nDLSystemMaxGainThr=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nULRxGain=30\nDLTxGain=28\nDLSystemGain=20\nCblCompFctr=9\nModemTxPwr=13\nUpperTempThr=40\nLowerTempThr=-5\nTempOffset=15\nAgcInterval=1000"

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
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_restore");
        return CRS_FAILURE;
    }

    if(threshCache == NULL){
        CRS_LOG(CRS_ERR,"\r\nthreshCache was NULL when calling Thresh_restore");

        nvsClose();

        return CRS_FAILURE;
        //Thresh_init();
    }

    CRS_free(&threshCache);
//    CLI_cliPrintf("\r\nsize of thrsh file - 1: %d", sizeof(THRSH_FILE) - 1);
    threshCache = CRS_realloc(threshCache, sizeof(THRSH_FILE)); // one more on purpose for end byte
    if (NULL == threshCache){
        CRS_LOG(CRS_ERR, "\r\nthreshCache realloc failed!");

        return CRS_FAILURE;
    }
    memset(threshCache, '\0', sizeof(THRSH_FILE));
    memcpy(threshCache, THRSH_FILE, sizeof(THRSH_FILE) - 1); // write into RAM

    CRS_retVal_t retStatus = Vars_setFile(&threshHandle, threshCache);
    nvsClose();
    if (retStatus != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nVars_setFile failed");
    }
    return retStatus;


}

CRS_retVal_t Thresh_init(){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_init");
        return CRS_FAILURE;
    }

    CRS_retVal_t status = CRS_FAILURE;
    int length = Vars_getLength(&threshHandle);
    if(length == -1){  //there is no hdr found in flash
        bool ret = Vars_createFile(&threshHandle);
        if(!ret){
            nvsClose();

            return CRS_FAILURE;
        }
        length = 1;
        threshCache = CRS_calloc(length, sizeof(char));
        if (NULL == threshCache){
            CRS_LOG(CRS_ERR, "\r\threshCache calloc failed!");
            nvsClose();
            return CRS_FAILURE;
        }
        memset(threshCache, '\0', length);
        status = Thresh_restore();
    }else{
        threshCache = CRS_calloc(length, sizeof(char));
        if (NULL == threshCache){
            CRS_LOG(CRS_ERR, "\r\threshCache calloc failed!");
            nvsClose();
            return CRS_FAILURE;
        }
        memset(threshCache, '\0', length);
        status = Vars_getFile(&threshHandle, threshCache);
    }
    nvsClose();

    return status;
}

CRS_retVal_t Thresh_read(char *vars, char *returnedVars){


    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_read");
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
            memcpy(returnedVars, threshCache, strlen(threshCache));
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
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_write");
        return CRS_FAILURE;
    }

    uint32_t length = 0;


    NVS_Attrs threshRegionAttrs;
    NVS_getAttrs(threshHandle, &threshRegionAttrs);

    threshCache = CRS_realloc(threshCache, threshRegionAttrs.regionSize - VARS_HDR_SZ_BYTES);  //TODO check why this is indeed
    if (threshCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nthreshCache failed to realloc!");
        nvsClose();

        return CRS_FAILURE;
    }
    length = Vars_setFileVars(&threshHandle, threshCache, vars);
    if (length == 0)
    {
        CRS_LOG(CRS_ERR,"\r\nVars_setFileVars failed");
        nvsClose();
    }
    threshCache = CRS_realloc(threshCache, length);
    if (threshCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nthreshCache failed to realloc!");
        nvsClose();

        return CRS_FAILURE;
    }
    nvsClose();

//    char filecopy [1024] = {0};
//    NVS_read(threshHandle, 0, (void*) filecopy, 1024);
    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_delete(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_delete");
        return CRS_FAILURE;
    }
    uint32_t length;


    NVS_Attrs threshRegionAttrs;
    NVS_getAttrs(threshHandle, &threshRegionAttrs);

    threshCache = CRS_realloc(threshCache, threshRegionAttrs.regionSize - VARS_HDR_SZ_BYTES);  //TODO check why this is indeed
    if (threshCache==NULL) {
        CRS_LOG(CRS_ERR,"\r\nthreshCache failed to realloc!");
        nvsClose();
        return CRS_FAILURE;
    }
    length = Vars_removeFileVars(&threshHandle, threshCache, vars);
    if(0 == length){
        CRS_LOG(CRS_ERR, "\r\nVars_removeFileVars failed!");

        nvsClose();

        return CRS_FAILURE;
    }

    threshCache = CRS_realloc(threshCache, length);
    if (NULL == threshCache){
        CRS_LOG(CRS_ERR, "\r\nthreshCache realloc failed!");
          nvsClose();

        return CRS_FAILURE;
    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_format(){

    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nnvsInit failed for Thresh_format");

        return CRS_FAILURE;
    }


    bool ret = Vars_createFile(&threshHandle);
    if (ret == false){
        CRS_LOG(CRS_ERR, "\r\nVars_createFile failed!");
        nvsClose();
        return CRS_FAILURE;
    }

    CRS_free(&threshCache);
    threshCache = CRS_calloc(1, sizeof(char));
    if (NULL == threshCache){
        CRS_LOG(CRS_ERR, "\r\nthreshCache calloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }

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

