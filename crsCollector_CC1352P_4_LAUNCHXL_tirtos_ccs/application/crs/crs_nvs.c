/*
 * crs_nvs.c
 *
 *  Created on: 20 ????? 2021
 *      Author: cellium
 */

#include "crs_nvs.h"

#define ENV_FILENAME "env.txt"
#define ENV_FILE_IDX (MAX_FILES - 50)

#define TRSH_FILENAME "trsh.txt"
#define TRSH_FILE_IDX (MAX_FILES - 10)



static NVS_Handle gNvsHandle;
static NVS_Attrs gRegionAttrs;
static Semaphore_Handle collectorSem;
static uint8_t gFAT_sector_sz;
static uint32_t gNumFiles;
static void printStatus(int_fast16_t retStatus);
static CRS_retVal_t Nvs_readFAT(CRS_FAT_t *fat);
static CRS_retVal_t Nvs_readFAT_64files(CRS_FAT_t *fat, int i);
static CRS_retVal_t Nvs_writeFAT(CRS_FAT_t *fat);
static CRS_retVal_t Nvs_writeFATRecord(CRS_FAT_t *fat, uint32_t location);
static CRS_retVal_t Nvs_writeFAT_64files(CRS_FAT_t *fat, int i);
static CRS_retVal_t Nvs_readFATNumFiles(CRS_FAT_t *fat, uint32_t start,
                                        uint32_t numFiles);
static CRS_retVal_t isVarsFileExists(int file_type);

static void getVars(char * file, char * keys, char * values);
static void setVars(char * file, char * vars, const char * d);

static int hex2int(char ch);

static CRS_FAT_t FATcache[FAT_CACHE_SZ+1] = {0};
char *bufCache = NULL;

CRS_retVal_t Nvs_init(void *sem)
{
    collectorSem = sem;
    NVS_Params nvsParams;
    NVS_init();
    NVS_Params_init(&nvsParams);
    gNvsHandle = NVS_open(CONFIG_NVSEXTERNAL, &nvsParams);

    if (gNvsHandle == NULL)
    {
        CLI_cliPrintf("NVS_open() failed.\r\n");

        return (NULL);
    }
    /*
     * This will populate a NVS_Attrs structure with properties specific
     * to a NVS_Handle such as region base address, region size,
     * and sector size.
     */
    NVS_getAttrs(gNvsHandle, &gRegionAttrs);
    uint32_t numFiles = (gRegionAttrs.regionSize / gRegionAttrs.sectorSize);
    gFAT_sector_sz = (numFiles * sizeof(CRS_FAT_t)
            + (gRegionAttrs.sectorSize - 1)) / gRegionAttrs.sectorSize;
    gFAT_sector_sz++;
    gNumFiles = numFiles - gFAT_sector_sz;
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    CRS_retVal_t trsh = isVarsFileExists(1);
    if (trsh != CRS_SUCCESS){
#ifndef CLI_SENSOR
        Nvs_createVarsFile("Technology=5G\nBand=l48\nBandwidth=20\nEARFCN=55340\nCentralFreq=3560\nMIMO=2x2\nMaxCableLoss=35\nDLMaxInputPower=-35\nDLRxMaxGain=45\nULTxMaxGain=55\nULMaxOutputPower=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nUnitType=CDU", 1);
#else
        Nvs_createVarsFile("Technology=5G\nBand=l48\nBandwidth=20\nEARFCN=55340\nCentralFreq=3560\nMIMO=2x2\nMaxCableLoss=35\nULMaxInputPower=-35\nULRxMaxGain=60\nDLTxMaxGain=55\nDLMaxOutputPower=20\nDLSystemMaxGain=20\nSyncMode=Manual\nCPType=Normal\nSensorMode=0\nUnitType=CRU", 1);
#endif
    }
    CRS_retVal_t rsp =  isVarsFileExists(0);
    if (rsp == CRS_SUCCESS)
    {
        return CRS_SUCCESS;
    }
#ifndef CLI_SENSOR

    Nvs_createVarsFile("name=Cdu\nver=0\nconfig=0\nimg=0", 0);
#else
    Nvs_createVarsFile("name=Cru\nver=0\nconfig=0\nimg=0", 0);

#endif

    return CRS_SUCCESS;

}

static CRS_retVal_t isVarsFileExists(int file_type)
{
    int file_index;
    if(file_type){
        file_index = TRSH_FILE_IDX;
    }else{
        file_index = ENV_FILE_IDX;
    }
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle,
                 (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                 (void*) strlenStr, STRLEN_BYTES);

    if (strlen(strlenStr) > 3)
    {
        return CRS_FAILURE;

    }
    if(hex2int(strlenStr[0]) == -1 && strlenStr[0] != 0)
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


CRS_retVal_t Nvs_ls()
{

    /* Display the NVS region attributes. */
    CLI_cliPrintf("\r\nMaxFileSize=0x%x", gRegionAttrs.sectorSize);
    CLI_cliPrintf("\r\nFlashSize=0x%x", gRegionAttrs.regionSize);
    CRS_FAT_t fat[MAX_FILES];
    Nvs_readFAT((fat));
    char strlenStr[STRLEN_BYTES] = { 0 };
    uint32_t strlen;
    int i = 0;
    int numfiles = 0;

    while (i < MAX_FILES)
    {
        if (fat[i].isExist == true)
        {
            NVS_read(gNvsHandle,
                     (fat[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                     (void*) strlenStr, STRLEN_BYTES);
            sscanf(strlenStr, "%x", &strlen);
            CLI_cliPrintf("\r\nName=%s Size=0x%x", fat[i].filename,
                          strlen);
            numfiles++;
            strlen = 0;
            memset(strlenStr, 0, STRLEN_BYTES);
        }

        i++;
    }

    CLI_cliPrintf("\r\nAvblFilesSlots=0x%x",
                  gNumFiles - numfiles);

}


CRS_retVal_t Nvs_rmVarsFile(char* vars, int file_type)
{
    int file_index;
    if(file_type){
        file_index = TRSH_FILE_IDX;
    }else{
        file_index = ENV_FILE_IDX;
    }
    char fileContent[1148] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle,
             (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) fileContent, 512);
    memcpy(strlenStr, fileContent, STRLEN_BYTES);
    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
    if (strlen(strlenStr) > 3)
        {
            return CRS_FAILURE;
        }
    if (strlenPrev > 1024)
    {
        return CRS_FAILURE;
    }
    if (strlenPrev > 500)
    {
        NVS_read(
                gNvsHandle,
                (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize + 512,
                (void*) fileContent + 512, 512);
    }

    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
    char copyFile[1148] = { 0 };

    const char s[2] = " ";
    char *key;
    char * delim;
    char * key_indx;
    char * token = strtok(vars, s);
    char * tok_cont = token + strlen(token) + 1;
    while (token){
        key = strtok(copyFileContent, "\n");
        while(key){
            delim = strstr(key, "=");
            if(delim){
                *delim = '\0';
                if(strcmp(key, token) == 0){
                    *delim = '=';
                    memset(key, 255, strlen(key));
                    key_indx = key;
                    key = NULL;
                }
                else{
                    *delim = '=';
                     key_indx = key;
                     key = strtok(NULL, "\n");
                }
            }
            else{
                key_indx = key;
                key = strtok(NULL, "\n");
            }
        *(key_indx + strlen(key_indx)) = '\n';
        }
        if(tok_cont < vars + MAX_LINE_CHARS){
            token = strtok(tok_cont, s);
            tok_cont = token + strlen(token) + 1;
        }
        else{
            token = NULL;
        }
    }
    const char d[2] = "\n";
    token = strtok(copyFileContent, d);
    while (token != NULL)
    {
        if(strstr(token, "=")){
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
    size_t startFile = ((file_index + gFAT_sector_sz)
            * gRegionAttrs.sectorSize);
    NVS_write(gNvsHandle, startFile, (void*) fileContent,
              STRLEN_BYTES + newStrlen + 3, NVS_WRITE_ERASE);

    return CRS_SUCCESS;
}

static void setVars(char * file, char * vars, const char * d){
    bool is_before = false;
    char copyFile [512] = {0};
    memcpy(copyFile, file, 512);
    char * key;
    char * delim;
    char * tk_delim;
    char * token = strtok(vars, d);
    char * tok_cont = token + strlen(token) + 1;
    while (token){
        tk_delim = strstr(token, "=");
        if(!tk_delim || tk_delim==token){
            token = strtok(NULL, d);
            tok_cont = token + strlen(token) + 1;
            continue;
        }
        key = strtok(copyFile, "\n");
        while(key){
            delim = strstr(key ,"=");
            if(delim){
                *delim = '\0';
                *tk_delim = '\0';
                if(strcmp(key, token) == 0){
                    is_before = true;
                }
                *delim = '=';
                *tk_delim = '=';
            }
            key = strtok(NULL, "\n");
        }
        if(!is_before){
            strcat(file, token);
            strcat(file, "\n");
        }
        is_before = false;
        memcpy(copyFile, file, 512);
        if(tok_cont < vars + MAX_LINE_CHARS){
            token = strtok(tok_cont, d);
            tok_cont = token + strlen(token) + 1;
        }else{
            token = NULL;
        }
    }
}

static void getVars(char * file, char * keys, char * values){
    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array
    const char s[2] = " ";

    memset(values, 0, MAX_LINE_CHARS);
    char *key;
    char * delim;
    char *token = strtok(keys, s);
    char * tok_cont = token + strlen(token) + 1;
    char copyFile [512] = {0};
    memcpy(copyFile, file, 512);

    while (token != NULL){
        key = strtok(copyFile, "\n");
        while(key){
            delim = strstr(key, "=");
            if(delim){
                *delim = '\0';
                if(strcmp(key, token) == 0){
                    *delim = '=';
                    strcat(values, key);
                    strcat(values, "\n");
                }
                *delim = '=';
            }
            key = strtok(NULL, "\n");
        }
        memcpy(copyFile, file, 512);
        if(tok_cont < keys + MAX_LINE_CHARS){
            token = strtok(tok_cont, s);
            char * tok_cont = token + strlen(token) + 1;
        }
        else{
            token = NULL;
        }

    }
}

CRS_retVal_t Nvs_setVarsFile(char* vars, int file_type)
{
    int file_index;
    if(file_type){
        file_index = TRSH_FILE_IDX;
    }else{
        file_index = ENV_FILE_IDX;
    }
    char fileContent[1148] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle,
             (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) fileContent, 512);
    memcpy(strlenStr, fileContent, STRLEN_BYTES);
    if (strlen(strlenStr) > 3)
        {
            return CRS_FAILURE;
        }
    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
    if (strlenPrev > 1024)
    {
        return CRS_FAILURE;
    }
    if (strlenPrev > 500)
    {
        NVS_read(
                gNvsHandle,
                (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize + 512,
                (void*) fileContent + 512, 512);
    }

    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
    char copyFile[512] = { 0 };

    const char s[2] = " ";

    setVars(copyFile, vars, s);
    const char d[2] = "\n";

    setVars(copyFile, copyFileContent ,d);

    memset(fileContent, 0, 1100);
    uint32_t newStrlen =   strlen(copyFile);
    memset(strlenStr, 0, STRLEN_BYTES);
    int2hex(newStrlen, strlenStr);

    memcpy(fileContent, strlenStr, STRLEN_BYTES);
    memcpy(fileContent + STRLEN_BYTES + 1, copyFile, newStrlen);
    size_t startFile = ((file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize);
    NVS_write(gNvsHandle, startFile, (void*) fileContent, STRLEN_BYTES+newStrlen+3, NVS_WRITE_ERASE);

    return CRS_SUCCESS;




}
/**
 * file type 1 : TRSH_FILE_IDX
 *
 * file type 0 : ENV_FILE_IDX
 */
CRS_retVal_t Nvs_treadVarsFile(char * vars, int file_type)
{
    int file_index;
    if(file_type){
        file_index = TRSH_FILE_IDX;
    }else{
        file_index = ENV_FILE_IDX;
    }

    char fileContent[1148] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };

    NVS_read(gNvsHandle,
             (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) fileContent, 512);
    memcpy(strlenStr, fileContent, STRLEN_BYTES);
    if (strlen(strlenStr) > 3)
    {
        return CRS_FAILURE;
    }
    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
    if (strlenPrev > 1024)
    {
        return CRS_FAILURE;
    }
    if (strlenPrev > 500)
    {
        NVS_read(gNvsHandle,
                     (file_index + gFAT_sector_sz) * gRegionAttrs.sectorSize + 512,
                     (void*) fileContent + 512, 512);
    }
    if(strlen(vars)){
            // if names of variables are not empty
            char names[MAX_LINE_CHARS] = { 0 };
            memcpy(names, vars, MAX_LINE_CHARS);
            memset(vars, 0, MAX_LINE_CHARS);
            getVars((fileContent+STRLEN_BYTES+1), names, vars);

        }
        else{
            memcpy(vars, fileContent+STRLEN_BYTES+1, strlen(fileContent+STRLEN_BYTES+1));
        }
    return CRS_SUCCESS;



}




CRS_retVal_t Nvs_createVarsFile(char* vars, int file_type)
{   int file_index;
    if(file_type){
        file_index = TRSH_FILE_IDX;
    }else{
        file_index = ENV_FILE_IDX;
    }
    // char strlenStr[STRLEN_BYTES+3] = { 0 };
    char temp[4096] = { 0 };

    size_t startFile = ((file_index + gFAT_sector_sz)
            * gRegionAttrs.sectorSize);
    // write '0' char to first byte (the length of the file)
    temp[0] = '0';


    // write 0 to all file bytes.
    NVS_write(gNvsHandle, startFile, (void*) temp,
              4096, NVS_WRITE_ERASE);

    char init_vars [MAX_LINE_CHARS] = {0};

    if(strlen(vars) < MAX_LINE_CHARS){
        memcpy(init_vars, vars, strlen(vars));
    }
    else{
        memcpy(init_vars, vars, MAX_LINE_CHARS);
    }

    return Nvs_setVarsFile(init_vars, file_type);
}


CRS_retVal_t Nvs_writeFile(char *filename, char *buff)
{
    char temp[1024] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };
    memcpy(temp, buff, strlen(buff));
//    strcat(temp, "\n");

    int i = 0;
    Bool isAppend = false;
    //looking for a exsiting filename
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i%FAT_CACHE_SZ].filename) == strlen(filename) && memcmp(FATcache[i%FAT_CACHE_SZ].filename, filename, strlen(filename)) == 0)
        {
            isAppend = true;
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if (memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0)
            {
                isAppend = true;
                break;
            }
        }
        i++;

    }

    //if no file already exist with the same name
    if (isAppend == false)
    {
        CRS_FAT_t fat[MAX_FILES];
        Nvs_readFAT((fat));
        i = 0;
        while (fat[i].isExist == true)
        {
            i++;
            if (i == MAX_FILES)
            {
                CLI_cliPrintf("no available slots!\r\n");
                return CRS_FAILURE;
            }
        }
        int2hex(strlen(temp), strlenStr);
        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, STRLEN_BYTES,
                  NVS_WRITE_ERASE);
        NVS_write(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (STRLEN_BYTES + 1),
                (void*) temp, strlen(temp), 0);
        memset(fat[i].filename,0,FILENAME_SZ);
        memcpy(fat[i].filename, filename, strlen(filename));
//        fat[i].len = strlen(temp);
        fat[i].index = i + 1;
        fat[i].isExist = true;
        Nvs_writeFAT(fat);
        return CRS_SUCCESS;
    }
    //we append to the existing file
    else
    {
        char fileContent[4096] = { 0 };
        NVS_read(gNvsHandle,
                 (FATcache[i%FAT_CACHE_SZ].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                 (void*) strlenStr, STRLEN_BYTES);

        uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
        if (strlenPrev + strlen(buff) > 4000)
        {
            return CRS_FAILURE;
        }
        NVS_read(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (STRLEN_BYTES + 1),
                (void*) fileContent, strlenPrev);
        memset(strlenStr, 0, STRLEN_BYTES);

        uint32_t newStrlen = strlenPrev + strlen(temp);

        int2hex(newStrlen, strlenStr);

        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, STRLEN_BYTES,
                  NVS_WRITE_ERASE);
        strcat(fileContent, temp);
        size_t startFile = ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                + STRLEN_BYTES + 1;
        NVS_write(gNvsHandle, startFile, (void*) fileContent, newStrlen, 0);
//        fat[i].len += strlen(temp);
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_writeTempFile(char *filename, char *buff)
{
    char temp[1024] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };
    memcpy(temp, buff, strlen(buff));
    strcat(temp, "\n");

    int i = 0;
    //if no file already exist with the same name

    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    i = 0;
    while ((memcmp(FATcache[i].filename, "temp", strlen("temp")) != 0))
    {

        if (i == MAX_FILES)
        {
            CLI_cliPrintf("no available slots!\r\n");
            return CRS_FAILURE;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
            //            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }
        i++;
    }

    char fileContent[5096] = { 0 };
    NVS_read(gNvsHandle,
             (FATcache[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, STRLEN_BYTES);

    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
    if (strlenPrev > 0x1000)
    {
        CLI_cliPrintf("\r\nTEMP is above 4k!!\r\n");
        return CRS_SUCCESS;
    }

    NVS_read(
            gNvsHandle,
            ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                    + (STRLEN_BYTES + 1),
            (void*) fileContent, strlenPrev);
    memset(strlenStr, 0, STRLEN_BYTES);

    uint32_t newStrlen = strlenPrev + strlen(temp);
    if (newStrlen > 0x1000)
       {
           CLI_cliPrintf("\r\nTEMP is above 4k!!\r\n");
           return CRS_SUCCESS;
       }
    int2hex(newStrlen, strlenStr);

    NVS_write(gNvsHandle, (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
              (void*) strlenStr, STRLEN_BYTES,
              NVS_WRITE_ERASE);
    strcat(fileContent, temp);
    size_t startFile = ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + STRLEN_BYTES + 1;
    NVS_write(gNvsHandle, startFile, (void*) fileContent, newStrlen, 0);
//        fat[i].len = strlen(temp);
//            fat[i].index = i + 1;
//            fat[i].isExist = true;
//    Nvs_writeFAT(fat);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_JsonFile(char *filename, char *buff)
{
    char temp[600] = { 0 };
    char strlenStr[STRLEN_BYTES] = { 0 };
    memcpy(temp, buff, strlen(buff));
//    strcat(temp, "\n");

    int i = 0;
    Bool isAppend = false;
    //looking for a exsiting filename
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (memcmp(FATcache[i].filename, filename, strlen(filename)) == 0)
        {
            isAppend = true;
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }
        i++;
    }

    //if no file already exist with the same name
    if (isAppend == false)
    {
        CRS_FAT_t fat[MAX_FILES];
        Nvs_readFAT((fat));
        i = 0;
        while (fat[i].isExist == true)
        {
            i++;
            if (i == MAX_FILES)
            {
                CLI_cliPrintf("no available slots!\r\n");
                return CRS_FAILURE;
            }
        }
        int2hex(strlen(temp), strlenStr);
        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, STRLEN_BYTES,
                  NVS_WRITE_ERASE);
        NVS_write(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (STRLEN_BYTES + 1),
                (void*) temp, strlen(temp), 0);
        memcpy(fat[i].filename, filename, strlen(filename));
//        fat[i].len = strlen(temp);
        fat[i].index = i + 1;
        fat[i].isExist = true;
        Nvs_writeFAT(fat);
        return CRS_SUCCESS;
    }
    //we append to the existing file
    else
    {
        char fileContent[4096] = { 0 };
        NVS_read(gNvsHandle,
                 (FATcache[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                 (void*) strlenStr, STRLEN_BYTES);

        uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
        NVS_read(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (STRLEN_BYTES + 1),
                (void*) fileContent, strlenPrev);
        memset(strlenStr, 0, STRLEN_BYTES);

        uint32_t newStrlen = strlenPrev + strlen(temp);

        int2hex(newStrlen, strlenStr);

        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, STRLEN_BYTES,
                  NVS_WRITE_ERASE);
        strcat(fileContent, temp);
        size_t startFile = ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                + STRLEN_BYTES + 1;
        NVS_write(gNvsHandle, startFile, (void*) fileContent, newStrlen, 0);
//        fat[i].len += strlen(temp);
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_cat(char *filename)
{
    char strlenStr[STRLEN_BYTES] = { 0 };
    CRS_FAT_t fat[MAX_FILES];
    Nvs_readFAT((fat));
    int i = 0;
    while (i < MAX_FILES)
    {
        if (memcmp(fat[i].filename, filename, strlen(filename)) == 0)
        {
            break;
        }
        i++;
    }
    if (i == MAX_FILES)
    {
        CLI_cliPrintf("\r\nfile not found!\r\n");
        return CRS_SUCCESS;
    }
    char fileContent[4096] = { 0 };
    NVS_read(gNvsHandle,
             (fat[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, STRLEN_BYTES);

    uint32_t strlen;
    sscanf(strlenStr, "%x", &strlen);
    size_t startFile = ((fat[i].index + gFAT_sector_sz)
            * gRegionAttrs.sectorSize) + (STRLEN_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) fileContent, strlen);
//    char line[50] = { 0 };
    const char s[2] = "\n";
    char *token;
    token = strtok((fileContent), s);
    while (token != NULL)
    {
        CLI_cliPrintf("\r\n%s", token);
        token = strtok(NULL, s);
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_rm(char *filename)
{
    CRS_FAT_t fat[MAX_FILES];
    Nvs_readFAT((fat));
    int i = 0;
    while (i < gNumFiles)
    {
        if (memcmp(fat[i].filename, filename, strlen(filename)) == 0)
        {
            break;
        }
        i++;
    }
    if (i == gNumFiles)
    {
        CLI_cliPrintf("\r\nno such file\r\n");
        return CRS_SUCCESS;
    }
    fat[i].isExist = false;
    memset(fat[i].filename, 0, FILENAME_SZ);
    Nvs_writeFAT(fat);
    CLI_cliPrintf("\r\n%s deleted\r\n", filename);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_debug()
{
    CRS_FAT_t *fat;

    NVS_read(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t));

    char fileContent[4096] = { 0 };
    NVS_read(gNvsHandle,
             ((fat)->index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) fileContent, (fat)->len);
    readJson(fileContent);
//    int_fast16_t retStatus = NVS_write(gNvsHandle, (4096 * 2), (void* )buff, sizeof(buff),
//    NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
//    printStatus(retStatus);

//    char buffer[3000];
//    NVS_erase(gNvsHandle, 0, gRegionAttrs.sectorSize);
//    NVS_read(gNvsHandle, 0, (void*) buffer, sizeof(buffer));
//    CLI_cliPrintf("\r\nsize of fat struct: %d\r\nfat sector sz: %d",
//                  sizeof(CRS_FAT_t), gFAT_sector_sz);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_format()
{
    int_fast16_t retStatus=NVS_erase(gNvsHandle, 0, gRegionAttrs.regionSize);
    printStatus(retStatus);
}

static CRS_retVal_t Nvs_readFAT(CRS_FAT_t *fat)
{
    NVS_read(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t) * gNumFiles);
}

static CRS_retVal_t Nvs_readFATNumFiles(CRS_FAT_t *fat, uint32_t start,
                                        uint32_t numFiles)
{
    int_fast16_t retStatus= NVS_read(gNvsHandle, start * sizeof(CRS_FAT_t), (void*) fat,
             sizeof(CRS_FAT_t) * numFiles);
}

static CRS_retVal_t Nvs_readFAT_64files(CRS_FAT_t *fat, int sector)
{
    NVS_read(gNvsHandle, sector * gRegionAttrs.sectorSize, (void*) fat,
             gRegionAttrs.sectorSize);
}

static CRS_retVal_t Nvs_writeFAT(CRS_FAT_t *fat)
{
    NVS_write(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t) * gNumFiles,
    NVS_WRITE_ERASE);
}

static CRS_retVal_t Nvs_writeFATRecord(CRS_FAT_t *fat, uint32_t location)
{
    NVS_write(gNvsHandle, (location * sizeof(CRS_FAT_t)), (void*) fat,
              sizeof(CRS_FAT_t), 0);
}

static CRS_retVal_t Nvs_writeFAT_64files(CRS_FAT_t *fat, int i)
{

    NVS_write(gNvsHandle, 0, (void*) fat, i * gRegionAttrs.sectorSize,
    NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);

}

CRS_retVal_t Nvs_readLine(char *filename, uint32_t lineNumber, char *respLine)
{
    CRS_FAT_t fat;
    uint32_t i = 0;
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (memcmp(FATcache[i].filename, filename, strlen(filename)) == 0)
        {
            fat = FATcache[i];
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }
        i++;
    }

    if (i == MAX_FILES)
    {
        CLI_cliPrintf("\r\nfile not found!\r\n");
        return CRS_FAILURE;
    }
    char strlenStr[STRLEN_BYTES] = { 0 };
    uint32_t strlen_f;
    NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, STRLEN_BYTES);
    sscanf(strlenStr, "%x", &strlen_f);
    char fileContent[4096] = { 0 };
    size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + (STRLEN_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) fileContent, strlen_f);

    i = 0;
    const char *s = "\r\n";
    char *token;
    token = strtok((fileContent), s);
    while (token != NULL)
    {
        i++;
        if (i == lineNumber)
        {
            memcpy(respLine, token, strlen(token));
            return CRS_SUCCESS;
        }
        token = strtok(NULL, s);
    }

    if (lineNumber > i)
    {
//        OsalPort_free(bufCache);
        memcpy(respLine, "FAILURE", CRS_NVS_LINE_BYTES);
        return CRS_EOF;
    }
    memcpy(respLine, "FAILURE", CRS_NVS_LINE_BYTES);
    return CRS_FAILURE;
}

char* Nvs_readFileWithMalloc( char *filename)
{
//    CLI_printHeapStatus();
    CRS_FAT_t fat;
    char *respLine = NULL;
       uint32_t i = 0;
   //    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

       Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
       while (i < MAX_FILES)
       {

           if (strlen(FATcache[i%FAT_CACHE_SZ].filename) == strlen(filename) &&memcmp(FATcache[i%FAT_CACHE_SZ].filename, filename, strlen(filename)) == 0)
           {
   //            fat = FATcache[i];
               memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
               break;
           }
           else if ((i % FAT_CACHE_SZ) == 0 && i != 0)
           {
   //            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
               Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
           }

           if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
           {
               if (memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                          strlen(filename)) == 0)
               {
                   //            fat = FATcache[i];
                   memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                   break;
               }
           }
           i++;
       }



       if (i == MAX_FILES)
       {
           CRS_LOG(CRS_ERR,"File not found");

           return NULL;
       }
   //    CLI_cliPrintf("filenamefat:%s, filenameme:%s, sizeme:0x%x", fat.filename, filename, strlen(filename));

       char strlenStr[STRLEN_BYTES] = { 0 };
       uint32_t strlen_f;
       NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                (void*) strlenStr, STRLEN_BYTES);
       sscanf(strlenStr, "%x", &strlen_f);
       size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
               + (STRLEN_BYTES + 1);

       respLine =  CRS_malloc(strlen_f+100);
       if (respLine == NULL)
       {
           CRS_LOG(CRS_ERR,"Malloc failed");

           return NULL;

       }
       memset(respLine, 0,strlen_f + 100 );
       NVS_read(gNvsHandle, startFile, (void*) respLine, strlen_f);
       return respLine;
}

CRS_retVal_t Nvs_readFile(const char *filename, char *respLine)
{
    CRS_FAT_t fat;
    uint32_t i = 0;
//    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i%FAT_CACHE_SZ].filename) == strlen(filename) &&memcmp(FATcache[i%FAT_CACHE_SZ].filename, filename, strlen(filename)) == 0)
        {
//            fat = FATcache[i];
            memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if (memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0)
            {
                //            fat = FATcache[i];
                memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                break;
            }
        }
        i++;
    }



    if (i == MAX_FILES)
    {
        CRS_LOG(CRS_ERR,"File not found");
        return CRS_FAILURE;
    }
//    CLI_cliPrintf("filenamefat:%s, filenameme:%s, sizeme:0x%x", fat.filename, filename, strlen(filename));

    char strlenStr[STRLEN_BYTES] = { 0 };
    uint32_t strlen_f;
    NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, STRLEN_BYTES);
    sscanf(strlenStr, "%x", &strlen_f);
    size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + (STRLEN_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) respLine, strlen_f);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_insertSpecificLine(char *filename, const char *line,
                                    uint32_t lineNum)
{

}


CRS_retVal_t Nvs_isFileExists(char *filename)
{
    CRS_FAT_t fat;
      uint32_t i = 0;
  //    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

      Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
      while (i < MAX_FILES)
      {

          if (strlen(FATcache[i%FAT_CACHE_SZ].filename) == strlen(filename) && memcmp(FATcache[i%FAT_CACHE_SZ].filename, filename, strlen(filename)) == 0)
          {
  //            fat = FATcache[i];
              memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
              break;
          }
          else if ((i % FAT_CACHE_SZ) == 0)
          {
  //            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
              Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
          }

          if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
          {
              if (memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                         strlen(filename)) == 0)
              {
                  //            fat = FATcache[i];
                  memcpy(&fat, &(FATcache[i%FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                  break;
              }
          }
          i++;
      }



      if (i == MAX_FILES)
      {
          return CRS_FAILURE;
      }
      return CRS_SUCCESS;

}

static void printStatus(int_fast16_t retStatus)
{
    if (retStatus == NVS_STATUS_SUCCESS)
    {
        CLI_cliPrintf("\r\nNVS_STATUS_SUCCESS");
    }
    if (retStatus == NVS_STATUS_ERROR)
    {
        CLI_cliPrintf("\r\nNVS_STATUS_ERROR");
    }
    if (retStatus == NVS_STATUS_INV_OFFSET)
    {
        CLI_cliPrintf("\r\nNVS_STATUS_INV_OFFSET");
    }
    if (retStatus == NVS_STATUS_INV_WRITE)
    {
        CLI_cliPrintf("\r\NVS_STATUS_INV_WRITE");
    }
    if (retStatus == NVS_STATUS_INV_ALIGNMENT)
    {
        CLI_cliPrintf("\r\NVS_STATUS_INV_ALIGNMENT");
    }
}
