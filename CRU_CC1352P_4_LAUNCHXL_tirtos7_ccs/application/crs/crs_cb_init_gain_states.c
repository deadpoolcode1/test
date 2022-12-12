/*
 * crs_cb_init_gain_states.c
 *
 *  Created on: 29 ????? 2022
 *      Author: cellium
 */



/*******************************************************************************
 Includes
 *****************************************************************************/

#include <ti/drivers/NVS.h>
#include <string.h>
#include "crs_cb_init_gain_states.h"
#include "crs_vars.h"
#include "crs_nvs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define STRLEN_BYTES 32
#define MAX_LINE_CHARS 1024
#define INIT_GAIN_STATES_FILENAME "init gain"
#define INIT_GAIN_STATES_FILE "init_dc_if_low_freq_tx_chip_1=NULL\ninit_dc_rf_high_freq_hb_rx_chip_0=NULL\ninit_uc_if_low_freq_rx_chip_1=NULL\ninit_uc_rf_high_freq_hb_tx_chip_0=NULL\ninit_dc_if_low_freq_tx_chip_3=NULL\ninit_dc_rf_high_freq_hb_rx_chip_2=NULL\ninit_uc_if_low_freq_rx_chip_3=NULL\ninit_uc_rf_high_freq_hb_tx_chip_2=NULL\ninit_dc_if_low_freq_tx_chip_5=NULL\ninit_dc_rf_high_freq_hb_rx_chip_4=NULL\ninit_uc_if_low_freq_rx_chip_5=NULL\ninit_uc_rf_high_freq_hb_tx_chip_4=NULL\ninit_dc_if_low_freq_tx_chip_7=NULL\ninit_dc_rf_high_freq_hb_rx_chip_6=NULL\ninit_uc_if_low_freq_rx_chip_7=NULL\ninit_uc_rf_high_freq_hb_tx_chip_6=NULL\n"


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t nvsInit();
static CRS_retVal_t nvsClose();

/******************************************************************************
 Local variables
 *****************************************************************************/

static char * initGainStatesCache  = NULL;
static NVS_Handle initGainStatesHandle = NULL;
static bool gIsMoudleInit = false;

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t CIGS_restore()
{
    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

    if(initGainStatesCache == NULL){
        nvsClose();
        return CRS_FAILURE;
        //CIGS_init();
    }

    if (Nvs_isFileExists(INIT_GAIN_STATES_FILENAME) == CRS_SUCCESS)
    {
        CRS_free(&initGainStatesCache);
        initGainStatesCache = Nvs_readFileWithMalloc(INIT_GAIN_STATES_FILENAME);
        if (!initGainStatesCache)
        {
            initGainStatesCache = CRS_calloc(1, sizeof(char));
        }
    }
    else
    {
        initGainStatesCache = CRS_realloc(initGainStatesCache, sizeof(INIT_GAIN_STATES_FILE));
        memcpy(initGainStatesCache, INIT_GAIN_STATES_FILE, sizeof(INIT_GAIN_STATES_FILE));

    }
    CRS_retVal_t status = Vars_setFile(&initGainStatesHandle, initGainStatesCache);

    nvsClose();
    return status;

}

CRS_retVal_t CIGS_init(){
    if (nvsInit() != CRS_SUCCESS)
    {
      CRS_LOG(CRS_ERR,"\r\nnvsInit failed for CIGS_init");
      return CRS_FAILURE;
    }

    CRS_retVal_t status = CRS_FAILURE;
    int length = Vars_getLength(&initGainStatesHandle);
    if(length == -1){
        bool ret = Vars_createFile(&initGainStatesHandle);
        if(!ret){
            nvsClose();
            return CRS_FAILURE;
        }
        length = 1;
        initGainStatesCache = CRS_calloc(length, sizeof(char));
        status = CIGS_restore();
    }else{
        initGainStatesCache = CRS_calloc(length, sizeof(char));
        status = Vars_getFile(&initGainStatesHandle, initGainStatesCache);
    }

    nvsClose();

    return status;
}

CRS_retVal_t CIGS_read(char *vars, char *returnedVars){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

//    if(initGainStatesCache == NULL){
//        CIGS_init();
//    }

    if(strlen(vars)){
        bool exists = Vars_getVars(initGainStatesCache, vars, returnedVars);
        if(!exists){
            nvsClose();
            return CRS_FAILURE;
        }
    }
    else{
        if(strlen(initGainStatesCache)){
            strcpy(returnedVars, initGainStatesCache);
            if(returnedVars[strlen(initGainStatesCache)-1] == '\n'){
                returnedVars[strlen(initGainStatesCache)-1] = 0;
            }
        }
    }

    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_write(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

//    if(initGainStatesCache == NULL){
//        CIGS_init();
//    }

    uint32_t length = 0;
    NVS_Attrs cigsRegionAttrs;
    NVS_getAttrs(initGainStatesHandle, &cigsRegionAttrs);

    initGainStatesCache = CRS_realloc(initGainStatesCache, cigsRegionAttrs.regionSize - STRLEN_BYTES);
    length = Vars_setFileVars(&initGainStatesHandle, initGainStatesCache, vars);

    initGainStatesCache = CRS_realloc(initGainStatesCache, length);
//    char filecopy [1024] = {0};
//    NVS_read(initGainStatesHandle, 0, (void*) filecopy, 1024);
    nvsClose();
    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_delete(char *vars){

    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

//    if(initGainStatesCache == NULL){
//        CIGS_init();
//    }

    uint32_t length = 0;
    NVS_Attrs cigsRegionAttrs;
    NVS_getAttrs(initGainStatesHandle, &cigsRegionAttrs);

    initGainStatesCache = CRS_realloc(initGainStatesCache, cigsRegionAttrs.regionSize - STRLEN_BYTES);
    length = Vars_removeFileVars(&initGainStatesHandle, initGainStatesCache, vars);
    if(!length){
        CRS_LOG(CRS_ERR, "\r\nCIGS_delete - Vars_removeFileVars failed");
        nvsClose();
        return CRS_FAILURE;
    }
    initGainStatesCache = CRS_realloc(initGainStatesCache, length);

    nvsClose();
    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_format(){
    if (nvsInit() != CRS_SUCCESS)
    {
        return CRS_FAILURE;
    }

//    if (initGainStatesHandle == NULL)
//    {
//        NVS_Params nvsParams;
//        NVS_init();
//        NVS_Params_init(&nvsParams);
//        initGainStatesHandle = NVS_open(INIT_GAIN_NVS, &nvsParams);
//    }

    bool ret = Vars_createFile(&initGainStatesHandle);
    CRS_free(&initGainStatesCache);
    initGainStatesCache = CRS_calloc(1, sizeof(char));
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

    initGainStatesHandle = NVS_open(INIT_GAIN_NVS, &nvsParams);

    if (initGainStatesHandle == NULL)
    {

        return (CRS_FAILURE);
    }

    gIsMoudleInit = true;
    return CRS_SUCCESS;
}

static CRS_retVal_t nvsClose()
{
    gIsMoudleInit = false;

    if (initGainStatesHandle == NULL)
    {
        return CRS_SUCCESS;
    }
    NVS_close(initGainStatesHandle);
    initGainStatesHandle = NULL;
    return CRS_SUCCESS;
}
