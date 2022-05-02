/*
 * crs_thresholds.c
 *
 *  Created on: 17 Mar 2022
 *      Author: epc_4
 */
#include "crs_thresholds.h"
#include "crs_nvs.h"

#include "crs_cli.h"
//#include "osal_port.h"
/* Driver Header files */
#include <ti/drivers/NVS.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define STRLEN_BYTES 32
#define MAX_LINE_CHARS 1024
#define THRSH_FILENAME "thrsh"
#define ENV_FILENAME "env"
#ifndef CLI_SENSOR
#define THRSH_FILE "Technology=5G\nBand=B48\nBandwidth=20\nEARFCN=55340\nCentralFreq=3560\nMIMO=2x2\n\
    MaxCableLossThr=35\nDLMaxInputPower=-35\nDLRxMaxGainThr=45\nULTxMaxGainThr=55\nULMaxOutputPower=20\n\
    SyncMode=Manual\nCPType=Normal\nSensorMode=0\nDLRxGain=28\nULTxGain=28\nTmpThr=40"
#define ENV_FILE "name=Cdu\nver=0\nconfig=0\nimg=0"
#else
#define THRSH_FILE "Technology=5G\nBand=B48\nBandwidth=20\nEARFCN=55340\nCentralFreq=3560\nMIMO=2x2\n\
    MaxCableLossThr=35\nULMaxInputPower=-35\nULRxMaxGainThr=60\nDLTxMaxGainThr=55\nDLMaxOutputPower=20\n\
    DLSystemMaxGainThr=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nULRxGain=30\nDLTxGain=28\n\
    DLSystemGain=20\nTmpThr=40"
#define ENV_FILE "name=Cru\nver=0\nconfig=0\nimg=0"
#endif

static NVS_Handle gNvsHandle;
static NVS_Attrs gRegionAttrs;

static CRS_retVal_t isVarsFileExists(int fileIndex);
static CRS_retVal_t createVarsFile(char *vars, int fileIndex);
static void setVars(char *file, char *vars, const char *d);
static void getVars(char *file, char *keys, char *values);
static int hex2int(char ch);

static CRS_retVal_t defaultVarFile(int fileIndex);

static char* gThreshCache = NULL;
static char* gEnvCache = NULL;
static char* gFileCache = NULL;


static CRS_retVal_t defaultVarFile(int fileIndex){
    CRS_retVal_t ret = CRS_FAILURE;
    if(fileIndex){
        if(Nvs_isFileExists(THRSH_FILENAME) == CRS_SUCCESS){
            gFileCache = Nvs_readFileWithMalloc(THRSH_FILENAME);
            if(gFileCache){
                ret = createVarsFile(gFileCache, fileIndex);
            }
        }
        else{
            ret = createVarsFile(THRSH_FILE, fileIndex);
        }
    }
    else{
        if(Nvs_isFileExists(ENV_FILENAME) == CRS_SUCCESS){
            gFileCache = Nvs_readFileWithMalloc(ENV_FILENAME);
            if(gFileCache){
                ret = createVarsFile(gFileCache, fileIndex);
            }
        }
        else{
            ret = createVarsFile(ENV_FILE, fileIndex);
        }
    }
    CRS_free(gFileCache);
    return ret;
}

CRS_retVal_t Thresh_restore(int fileIndex){
    Thresh_format(fileIndex);
    CRS_retVal_t ret = defaultVarFile(fileIndex);
    return ret;
}

CRS_retVal_t Thresh_init()
{
    NVS_Params nvsParams;
    NVS_init();
    NVS_Params_init(&nvsParams);
    gNvsHandle = NVS_open(CONFIG_NVS_0, &nvsParams);

    if (gNvsHandle == NULL)
    {
        CLI_cliPrintf("NVS_open() failed.\r\n");

        return CRS_FAILURE;
    }
    /*
     * This will populate a NVS_Attrs structure with properties specific
     * to a NVS_Handle such as region base address, region size,
     * and sector size.
     */
    NVS_getAttrs(gNvsHandle, &gRegionAttrs);
    uint32_t numFiles = (gRegionAttrs.regionSize / gRegionAttrs.sectorSize);

    CRS_retVal_t env = isVarsFileExists(0);
    if (env != CRS_SUCCESS)
    {
        defaultVarFile(0);
    }

    CRS_retVal_t trsh = isVarsFileExists(1);
    if (trsh != CRS_SUCCESS)
    {
        defaultVarFile(1);
    }

    return CRS_SUCCESS;
}

static CRS_retVal_t isVarsFileExists(int fileIndex)
{

    char strlenStr[STRLEN_BYTES + 1] = { 0 };

    NVS_read(gNvsHandle, fileIndex * gRegionAttrs.sectorSize, (void*) strlenStr,
             STRLEN_BYTES);

    if (strlen(strlenStr) > 3)
    {
        return CRS_FAILURE;

    }
    if (hex2int(strlenStr[0]) == -1 && strlenStr[0] != 0)
    {
        return CRS_FAILURE;
    }
    if (hex2int(strlenStr[1]) == -1 && strlenStr[1] != 0)
    {
        return CRS_FAILURE;
    }
    if (hex2int(strlenStr[2]) == -1 && strlenStr[2] != 0)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;

}

static CRS_retVal_t createVarsFile(char *vars, int fileIndex)
{
    // char strlenStr[STRLEN_BYTES+3] = { 0 };
    char temp[4096] = { 0 };

    size_t startFile = (fileIndex * gRegionAttrs.sectorSize);
    // write '0' char to first byte (the length of the file)
    temp[0] = '0';

    // write 0 to all file bytes.
    NVS_write(gNvsHandle, startFile, (void*) temp, 4096, NVS_WRITE_ERASE);

    char init_vars[MAX_LINE_CHARS] = { 0 };

    if (strlen(vars) < MAX_LINE_CHARS)
    {
        memcpy(init_vars, vars, strlen(vars));
    }
    else
    {
        memcpy(init_vars, vars, MAX_LINE_CHARS);
    }

    return Thresh_setVarsFile(init_vars, fileIndex);
}

CRS_retVal_t Thresh_format(int fileIndex)
{
    if(fileIndex){
        CRS_free(gThreshCache);
        gThreshCache = NULL;
    }
    else{
        CRS_free(gEnvCache);
        gEnvCache = NULL;
    }
    int_fast16_t retStatus = NVS_erase(gNvsHandle, fileIndex * gRegionAttrs.sectorSize, gRegionAttrs.sectorSize);
    return CRS_SUCCESS;

}


CRS_retVal_t Thresh_readVarsFile(char *vars, char *returnedVars, int fileIndex)
{
    char *fileContent = CRS_malloc( 1200);
    char *prevFileContnet = fileContent;
    if (fileContent == NULL)
    {
        return CRS_FAILURE;
    }
    memset(fileContent, 0, 1200);

    char strlenStr[STRLEN_BYTES+1] = { 0 };
    char * ptr;
    if(fileIndex){
        ptr = gThreshCache;
    }
    else{
        ptr = gEnvCache;
    }

    if (ptr == NULL)
    {
        NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize,
                     (void*) fileContent, MAX_LINE_CHARS);
            memcpy(strlenStr, fileContent, STRLEN_BYTES);
            if (strlen(strlenStr) > 3)
            {
                CRS_free(fileContent);

                return CRS_FAILURE;
            }
            uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
            if (strlenPrev > MAX_LINE_CHARS)
            {
                CRS_free(fileContent);

                return CRS_FAILURE;
            }
            if (strlenPrev > MAX_LINE_CHARS)
            {
                NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
                         (void*) fileContent + 512, 512);

            }
            fileContent = fileContent + STRLEN_BYTES + 1;
            ptr = CRS_malloc(strlen(fileContent) + 100);
            if (ptr == NULL)
            {
                CRS_free(prevFileContnet);

                return CRS_FAILURE;
            }
            memset(ptr, 0, strlen(fileContent) + 100);
            memcpy(ptr, fileContent, strlen(fileContent));

            if(fileIndex){
                gThreshCache = ptr;
            }
            else{
                gEnvCache = ptr;
            }
//            if(fileIndex){
//                CRS_free(gThreshCache);
//                gThreshCache = gTempCache;
//            }
//            else{
//                CRS_free(gEnvCache);
//                gEnvCache = gTempCache;
//            }

    }
    else
    {
        memcpy(fileContent, ptr, strlen(ptr));
    }


    if (vars != NULL && strlen(vars) > 0 && strlen(vars) < MAX_LINE_CHARS)
    {
        // if names of variables are not empty
        char names[MAX_LINE_CHARS+1] = { 0 };
        memcpy(names, vars, strlen(vars));
//        memset(vars, 0, MAX_LINE_CHARS);
        getVars(fileContent , names, returnedVars);

    }
    else
    {
        memcpy(returnedVars, fileContent ,
               strlen(fileContent ));
    }
    CRS_free(prevFileContnet);

    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_setVarsFile(char *vars, int fileIndex)
{

    char *fileContent = CRS_malloc(1200);
    char *prevFileContnet = fileContent;
    if (fileContent == NULL)
    {
        return CRS_FAILURE;
    }
    memset(fileContent, 0, 1200);
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize,
             (void*) fileContent, MAX_LINE_CHARS);
    memcpy(strlenStr, fileContent, STRLEN_BYTES);
    uint32_t strlenPrev;
    if (strlen(strlenStr) > 3)
    {
        CRS_free(fileContent);

        return CRS_FAILURE;
    }
    if(strlenStr[0] == '\0'){
        strlenPrev = 0;
    }
    else{
        strlenPrev = CLI_convertStrUint(strlenStr);
    }
    if (strlenPrev > MAX_LINE_CHARS)
    {
        CRS_free(fileContent);

        return CRS_FAILURE;
    }
    if (strlenPrev > MAX_LINE_CHARS)
    {
        NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
                 (void*) fileContent + 512, 512);
    }

    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
    char copyFile[MAX_LINE_CHARS] = { 0 };

    const char s[2] = " ";

    setVars(copyFile, vars, s);
    const char d[2] = "\n";

    setVars(copyFile, copyFileContent, d);

    memset(fileContent, 0, 1100);
    uint32_t newStrlen = strlen(copyFile);
    memset(strlenStr, 0, STRLEN_BYTES);
    int2hex(newStrlen, strlenStr);

    memcpy(fileContent, strlenStr, STRLEN_BYTES);
    memcpy(fileContent + STRLEN_BYTES + 1, copyFile, newStrlen);
    size_t startFile = ((fileIndex) * gRegionAttrs.sectorSize);
    NVS_write(gNvsHandle, startFile, (void*) fileContent,
              STRLEN_BYTES + newStrlen + 3, NVS_WRITE_ERASE);


    fileContent = fileContent + STRLEN_BYTES + 1;
    char * ptr;
    if(fileIndex){
        ptr = gThreshCache;
    }
    else{
        ptr = gEnvCache;
    }

    if (ptr == NULL)
    {
        ptr = CRS_malloc(strlen(fileContent ) + 100);
        if (ptr == NULL)
        {
            CRS_free(prevFileContnet);

            return CRS_FAILURE;
        }
        memset(ptr, 0, strlen(fileContent) + 100);
        memcpy(ptr, fileContent, strlen(fileContent));
    }
    else
    {
        CRS_free(ptr);
        ptr = CRS_malloc(strlen(fileContent) + 100);
        if (ptr == NULL)
        {
            CRS_free(prevFileContnet);

            return CRS_FAILURE;
        }
        memset(ptr, 0, strlen(fileContent) + 100);
        memcpy(ptr, fileContent, strlen(fileContent));
    }

//    if(fileIndex){
//        gThreshCache = gTempCache;
//    }
//    else{
//        gEnvCache = gTempCache;
//    }
//    gTempCache = NULL;
    if(fileIndex){
        gThreshCache = ptr;
    }
    else{
        gEnvCache = ptr;
    }
    CRS_free(prevFileContnet);

    return CRS_SUCCESS;
}

CRS_retVal_t Thresh_rmVarsFile(char *vars, int fileIndex)
{

    char *fileContent = CRS_malloc(1200);
    char *prevFileContnet = fileContent;
    if (fileContent == NULL)
    {
        return CRS_FAILURE;
    }
    memset(fileContent, 0, 1200);
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle, fileIndex * gRegionAttrs.sectorSize,
             (void*) fileContent, MAX_LINE_CHARS);
    memcpy(strlenStr, fileContent, STRLEN_BYTES);
    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
    if (strlen(strlenStr) > 3)
    {
        CRS_free(fileContent);

        return CRS_FAILURE;
    }
    if (strlenPrev > MAX_LINE_CHARS)
    {
        CRS_free(fileContent);

        return CRS_FAILURE;
    }
    if (strlenPrev > MAX_LINE_CHARS)
    {

        NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
                 (void*) fileContent + 512, 512);
    }

    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
    char copyFile[1148] = { 0 };

    const char s[2] = " ";
    char *key;
    char *delim;
    char *key_indx;
    char *token = strtok(vars, s);
    char *tok_cont = token + strlen(token) + 1;
    while (token)
    {
        key = strtok(copyFileContent, "\n");
        while (key)
        {
            delim = strstr(key, "=");
            if (delim)
            {
                *delim = '\0';
                if (strcmp(key, token) == 0)
                {
                    *delim = '=';
                    memset(key, 255, strlen(key));
                    key_indx = key;
                    key = NULL;
                }
                else
                {
                    *delim = '=';
                    key_indx = key;
                    key = strtok(NULL, "\n");
                }
            }
            else
            {
                key_indx = key;
                key = strtok(NULL, "\n");
            }
            *(key_indx + strlen(key_indx)) = '\n';
        }
        if (tok_cont < vars + MAX_LINE_CHARS)
        {
            token = strtok(tok_cont, s);
            tok_cont = token + strlen(token) + 1;
        }
        else
        {
            token = NULL;
        }
    }
    const char d[2] = "\n";
    token = strtok(copyFileContent, d);
    while (token != NULL)
    {
        if (strstr(token, "="))
        {
            strcat(copyFile, token);
            strcat(copyFile, "\n");
        }
        token = strtok(NULL, d);

    }

    memset(fileContent, 0, 1100);
    uint32_t newStrlen = strlen(copyFile);
    memset(strlenStr, 0, STRLEN_BYTES);
    int2hex(newStrlen, strlenStr);

    memcpy(fileContent, strlenStr, STRLEN_BYTES);
    memcpy(fileContent + STRLEN_BYTES + 1, copyFile, newStrlen);
    size_t startFile = ((fileIndex ) * gRegionAttrs.sectorSize);
    NVS_write(gNvsHandle, startFile, (void*) fileContent,
              STRLEN_BYTES + newStrlen + 3, NVS_WRITE_ERASE);

    fileContent = fileContent + STRLEN_BYTES + 1;
    char * ptr;
    if(fileIndex){
        ptr = gThreshCache;
    }
    else{
        ptr = gEnvCache;
    }

    if (ptr == NULL)
    {
        ptr = CRS_malloc(strlen(fileContent) + 100);
        if (ptr == NULL)
        {
            CRS_free(prevFileContnet);

            return CRS_FAILURE;
        }
        memset(ptr, 0, strlen(fileContent) + 100);
        memcpy(ptr, fileContent, strlen(fileContent));
    }
    else
    {
        CRS_free(ptr);
        ptr = CRS_malloc(strlen(fileContent) + 100);
        if (ptr == NULL)
        {
            CRS_free(prevFileContnet);

            return CRS_FAILURE;
        }
        memset(ptr, 0, strlen(fileContent) + 100);
        memcpy(ptr, fileContent, strlen(fileContent));
    }

//    if(fileIndex){
//        CRS_free(gThreshCache);
//        gThreshCache = gTempCache;
//    }
//    else{
//        CRS_free(gEnvCache);
//        gEnvCache = gTempCache;
//    }
    if(fileIndex){
        gThreshCache = ptr;
    }
    else{
        gEnvCache = ptr;
    }
    CRS_free(prevFileContnet);

    return CRS_SUCCESS;
}

static void setVars(char *file, char *vars, const char *d)
{
    bool is_before = false;
    char *key;
    char *delim;
    char *tk_delim;
    //char *tok_cont = token + strlen(token) + 1;
    char copyFile[strlen(file)+1];
    strcpy(copyFile, file);
    char copyFileContent[strlen(file)+1];
    strcpy(copyFileContent, file);
    char copyVars[strlen(vars)+1];
    strcpy(copyVars, vars);
    char *token = strtok(copyVars, d);
    while (token)
    {
        tk_delim = strstr(token, "=");
        if (!tk_delim || tk_delim == token)
        {
            token = strtok(NULL, d);
            //tok_cont = token + strlen(token) + 1;
            continue;
        }
        key = strtok(copyFile, "\n");
        while (key)
        {
            delim = strstr(key, "=");
            if (delim)
            {
                *delim = '\0';
                *tk_delim = '\0';
                if (strcmp(key, token) == 0)
                {
                    is_before = true;
                }
                *delim = '=';
                *tk_delim = '=';
            }
            key = strtok(NULL, "\n");
        }
        if (!is_before)
        {
            strcat(file, token);
            strcat(file, "\n");
        }
        is_before = false;
        strcpy(copyFile, copyFileContent);
        strcpy(copyVars, vars);
        key = strtok(copyVars, d);
        while(strcmp(key, token) != 0){
            key = strtok(NULL, d);
        }
        token = strtok(NULL, d);

    }
}

static void getVars(char *file, char *keys, char *values)
{
    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array
    const char s[2] = " ";

//    memset(values, 0, MAX_LINE_CHARS);
    char *key;
    char *delim;
    char copyFile[strlen(file)+1];
    strcpy(copyFile, file);
    char copyKeys[strlen(keys)+1];
    strcpy(copyKeys, keys);
    char *token = strtok(copyKeys, s);

    while (token != NULL)
    {
        key = strtok(copyFile, "\n");
        while (key)
        {
            delim = strstr(key, "=");
            if (delim)
            {
                *delim = '\0';
                if (strcmp(key, token) == 0)
                {
                    *delim = '=';
                    strcat(values, key);
                    strcat(values, "\n");
                    key = NULL;
                }
                *delim = '=';
            }
            key = strtok(NULL, "\n");
        }
        strcpy(copyFile, file);
        strcpy(copyKeys, keys);
        key = strtok(copyKeys, s);
        while(strcmp(key, token) != 0){
            key = strtok(NULL, s);
        }
        token = strtok(NULL, s);

    }
}


static int hex2int(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}
