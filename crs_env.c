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

#ifndef CLI_SENSOR

    #include "application/collector.h"

#else
    #include "application/sensor.h"
#endif

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define VARS_HDR_SZ_BYTES 32
#define MAX_LINE_CHARS 1024
#define MAC_ADDR_LEN 20
#define ENV_FILE_SZ 80

#define ENV_FILENAME "env"
#ifndef CLI_SENSOR
    #define ENV_FILE "ver=0\nconfig=0\nimg=0\nledMode=1"
#else
    #define ENV_FILE "ver=0\nconfig=0\nimg=0\nledMode=1"
#endif


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t nvsInit();
static CRS_retVal_t nvsClose();
static CRS_retVal_t getDefaultName(char macAddress [MAC_ADDR_LEN]);
static CRS_retVal_t convertExtAddrTo2Uint32(ApiMac_sAddrExt_t  *extAddr, uint32_t* left, uint32_t* right);
static CRS_retVal_t getEnvDefaultVals(char *defaultVals);


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

        status = Vars_getFile(&envHandle, envCache);
        nvsClose();
        if (status == CRS_SUCCESS)
        {

//#ifdef CLI_CEU_BP
//            Env_write("name=CEU_BP");
//#endif
//#ifdef CLI_CEU_CL
//            Env_write("name=CEU_CL");
//#endif
//#ifdef CLI_CRU
//            Env_write("name=CRU");
//#endif
//#ifdef CLI_CIU
//            Env_write("name=CIU");
//#endif

//
//        #ifndef CLI_SENSOR
//                  Env_write("name=Cdu");
//        #else
//                  Env_write("name=Cru");
//        #endif
        }
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
    envCache = CRS_realloc(envCache, ENV_FILE_SZ); // one more on purpose for end byte
    if (NULL == envCache){
        CRS_LOG(CRS_ERR, "\r\nenvCache realloc failed!");
        nvsClose();
        return CRS_FAILURE;
    }

    memset(envCache, '\0', ENV_FILE_SZ);

    char defaultVals [ENV_FILE_SZ] = {0};
    getEnvDefaultVals(defaultVals);

    memcpy(envCache, defaultVals, strlen(defaultVals)); //write into RAM cache
    CRS_retVal_t ret =  Vars_setFile(&envHandle, envCache); //write into flash
    if (ret != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR, "\r\nVars_setFile failed");
        nvsClose();
    }

    nvsClose();
    return ret;
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

static CRS_retVal_t convertExtAddrTo2Uint32(ApiMac_sAddrExt_t  *extAddr, uint32_t* left, uint32_t* right)
{
    uint32_t leftPart = 0, rightPart = 0;
    uint32_t mask = 0;

    rightPart = 0;
    leftPart = 0;
    rightPart |= (*extAddr)[7];

    mask |= (*extAddr)[6];
    mask = mask << 8;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[5];
    mask = mask << 16;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[4];
    mask = mask << 24;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[3];
    mask = mask << 0;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[2];
    mask = mask << 8;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[1];
    mask = mask << 16;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[0];
    mask = mask << 24;
    leftPart |= mask;
    mask = 0;

    *left = leftPart;
    *right = rightPart;
    return CRS_SUCCESS;
}

static CRS_retVal_t getDefaultName(char macAddress [MAC_ADDR_LEN])
{
    Llc_netInfo_t nwkInfo;
    uint32_t left = 0;
    uint32_t right = 0;
#ifndef CLI_SENSOR

    memset(macAddress, 0,MAC_ADDR_LEN);

    //        memset(&nwkInfo)
    bool rsp = Csf_getNetworkInformation(&nwkInfo);
    if (rsp == false)
    {
        return CRS_FAILURE;
    }
    else
    {
        convertExtAddrTo2Uint32(&(nwkInfo.devInfo.extAddress), &left, &right);
        //pan, longaddr,short,env.txt
        sprintf(macAddress,"0x%x%x", left, right);
    }


#else
   bool rsp = true;
   ApiMac_deviceDescriptor_t devInfo;
   rsp = Ssf_getNetworkInfo(&devInfo, &nwkInfo);
   if (rsp == false)
   {
       ApiMac_sAddrExt_t  mac = {0};
       getMac(&mac);
       convertExtAddrTo2Uint32(&(mac), &left, &right);
       sprintf(macAddress,"0x%x%x", left, right);

       return CRS_FAILURE;
   }
   else
   {
       convertExtAddrTo2Uint32(&(devInfo.extAddress), &left, &right);
       sprintf(macAddress,"0x%x%x", left, right);
   }


#endif
    return CRS_SUCCESS;

}


static CRS_retVal_t getEnvDefaultVals(char defaultVals[ENV_FILE_SZ])
{
    char defaultName [MAC_ADDR_LEN] = {0};
    getDefaultName(defaultName);

    sprintf(defaultVals, "name=%s\n", defaultName);

    memcpy(&defaultVals[strlen(defaultVals)], ENV_FILE, strlen(ENV_FILE));

    return CRS_SUCCESS;

}
