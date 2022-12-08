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
 Local variables
 *****************************************************************************/

static char * initGainStatesCache  = NULL;
static NVS_Handle initGainStatesHandle;

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t CIGS_restore()
{
    if(initGainStatesCache == NULL){
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
   return Vars_setFile(&initGainStatesHandle, initGainStatesCache);

}

CRS_retVal_t CIGS_init(){
    if (initGainStatesHandle == NULL)
    {
        NVS_Params nvsParams;
        NVS_init();
        NVS_Params_init(&nvsParams);
        initGainStatesHandle = NVS_open(INIT_GAIN_NVS, &nvsParams);

        if (initGainStatesHandle == NULL)
        {
             CLI_cliPrintf("NVS_open() failed.\r\n");
            return CRS_FAILURE;
        }
    }

    CRS_retVal_t status;
    int length = Vars_getLength(&initGainStatesHandle);
    if(length == -1){
        bool ret = Vars_createFile(&initGainStatesHandle);
        if(!ret){
            return CRS_FAILURE;
        }
        length = 1;
        initGainStatesCache = CRS_calloc(length, sizeof(char));
        status = CIGS_restore();
    }else{
        initGainStatesCache = CRS_calloc(length, sizeof(char));
        status = Vars_getFile(&initGainStatesHandle, initGainStatesCache);
    }

    return status;
}

CRS_retVal_t CIGS_read(char *vars, char *returnedVars){

    if(initGainStatesCache == NULL){
        CIGS_init();
    }
    if(strlen(vars)){
        bool exists = Vars_getVars(initGainStatesCache, vars, returnedVars);
        if(!exists){
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

    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_write(char *vars){

    uint32_t length;
    if(initGainStatesCache == NULL){
        CIGS_init();
    }

    NVS_Attrs cigsRegionAttrs;
    NVS_getAttrs(initGainStatesHandle, &cigsRegionAttrs);

    initGainStatesCache = CRS_realloc(initGainStatesCache, cigsRegionAttrs.regionSize);
    length = Vars_setFileVars(&initGainStatesHandle, initGainStatesCache, vars);

    initGainStatesCache = CRS_realloc(initGainStatesCache, length);
//    char filecopy [1024] = {0};
//    NVS_read(initGainStatesHandle, 0, (void*) filecopy, 1024);
    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_delete(char *vars){

    uint32_t length;
    if(initGainStatesCache == NULL){
        CIGS_init();
    }

    NVS_Attrs cigsRegionAttrs;
    NVS_getAttrs(initGainStatesHandle, &cigsRegionAttrs);

    initGainStatesCache = CRS_realloc(initGainStatesCache, cigsRegionAttrs.regionSize);
    length = Vars_removeFileVars(&initGainStatesHandle, initGainStatesCache, vars);
    if(!length){
        return CRS_FAILURE;
    }
    initGainStatesCache = CRS_realloc(initGainStatesCache, length);

    return CRS_SUCCESS;
}

CRS_retVal_t CIGS_format(){

    if (initGainStatesHandle == NULL)
    {
        NVS_Params nvsParams;
        NVS_init();
        NVS_Params_init(&nvsParams);
        initGainStatesHandle = NVS_open(INIT_GAIN_NVS, &nvsParams);
    }

    bool ret = Vars_createFile(&initGainStatesHandle);
    CRS_free(&initGainStatesCache);
    initGainStatesCache = CRS_calloc(1, sizeof(char));
    return CRS_SUCCESS;
}



