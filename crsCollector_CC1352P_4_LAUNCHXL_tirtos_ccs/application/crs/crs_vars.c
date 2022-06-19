/*
 * crs_vars.c
 *
 *  Created on: 12 Jun 2022
 *      Author: tomer_zur
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_thresholds.h"
#include "crs_nvs.h"
#include "crs_cli.h"
#include <stdlib.h>
/* Driver Header files */
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define STRLEN_BYTES 32
#define MAX_LINE_CHARS 1024
#define VAR_HEADER "TZKAM"

/******************************************************************************
 Local variables
 *****************************************************************************/

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
//static CRS_retVal_t isVarsFileExists(int fileIndex);
//static CRS_retVal_t createVarsFile(char *vars, int fileIndex);
//static int hex2int(char ch);

/******************************************************************************
 Public Functions
 *****************************************************************************/

//CRS_retVal_t Vars_restore(NVS_Handle * fileHandle, char * file)
//{
//    Vars_format(fileHandle, file);
//    CRS_retVal_t ret = Vars_defaultVarFile(fileHandle, file);
//    return ret;
//}

//CRS_retVal_t Thresh_init()
//{
//    NVS_Params nvsParams;
//    NVS_init();
//    NVS_Params_init(&nvsParams);
//    gNvsHandle = NVS_open(CONFIG_NVS_0, &nvsParams);
//
//    if (gNvsHandle == NULL)
//    {
//        CLI_cliPrintf("NVS_open() failed.\r\n");
//
//        return CRS_FAILURE;
//    }
//    /*
//     * This will populate a NVS_Attrs structure with properties specific
//     * to a NVS_Handle such as region base address, region size,
//     * and sector size.
//     */
//    NVS_getAttrs(gNvsHandle, &gRegionAttrs);
//    uint32_t numFiles = (gRegionAttrs.regionSize / gRegionAttrs.sectorSize);
//
//    CRS_retVal_t env = isVarsFileExists(0);
//    if (env != CRS_SUCCESS)
//    {
//        defaultVarFile(0);
//    }
//
//    CRS_retVal_t trsh = isVarsFileExists(1);
//    if (trsh != CRS_SUCCESS)
//    {
//        defaultVarFile(1);
//    }
//
//    return CRS_SUCCESS;
//}
//
CRS_retVal_t Vars_format(NVS_Handle *fileHandle)
{

    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    int_fast16_t retStatus = NVS_erase(*fileHandle, 0, fileRegionAttrs.sectorSize);
    if(retStatus != NVS_STATUS_SUCCESS){
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}
//
//CRS_retVal_t Vars_read(char *vars, char *returnedVars, NVS_Handle fileHandle, char * fileCache)
//{

//    NVS_Attrs fileRegionAttrs;
//    NVS_getAttrs(fileHandle, &fileRegionAttrs);
//
//    char strlenStr[STRLEN_BYTES] = { 0 };
//    NVS_read(fileHandle, 0,
//             (void*) strlenStr, STRLEN_BYTES);
//    char *fileContent = CRS_calloc(1200);
//    char *prevFileContnet = fileContent;
//    if (fileContent == NULL)
//    {
//        return CRS_FAILURE;
//    }
//    //char *ptr = fileCache;
//
//    if (fileCache == NULL)
//    {
//        NVS_read(fileHandle, 0,
//                 (void*) fileContent, fileRegionAttrs.sectorSize);
//        memcpy(strlenStr, fileContent, STRLEN_BYTES);
//        if (strlen(strlenStr) > 3)
//        {
//            CRS_free(fileContent);
//
//            return CRS_FAILURE;
//        }
//        uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
//        if (strlenPrev > MAX_LINE_CHARS)
//        {
//            CRS_free(fileContent);
//
//            return CRS_FAILURE;
//        }
//        if (strlenPrev > MAX_LINE_CHARS)
//        {
//            NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
//                     (void*) fileContent + 512, 512);
//
//        }
//        fileContent = fileContent + STRLEN_BYTES + 1;
//        ptr = CRS_malloc(strlen(fileContent) + 100);
//        if (ptr == NULL)
//        {
//            CRS_free(prevFileContnet);
//
//            return CRS_FAILURE;
//        }
//        memset(ptr, 0, strlen(fileContent) + 100);
//        memcpy(ptr, fileContent, strlen(fileContent));
//
//        if (fileIndex)
//        {
//            gThreshCache = ptr;
//        }
//        else
//        {
//            gEnvCache = ptr;
//        }
//
//    }
//    else
//    {
//        memcpy(fileContent, ptr, strlen(ptr));
//    }
//
//    if (vars != NULL && strlen(vars) > 0 && strlen(vars) < MAX_LINE_CHARS)
//    {
//        // if names of variables are not empty
//        char names[MAX_LINE_CHARS + 1] = { 0 };
//        memcpy(names, vars, strlen(vars));
////        memset(vars, 0, MAX_LINE_CHARS);
//        Vars_getVars(fileContent, names, returnedVars);
//
//    }
//    else
//    {
//        memcpy(returnedVars, fileContent, strlen(fileContent));
//    }
//    CRS_free(prevFileContnet);
//
//    return CRS_SUCCESS;
//}
//
//CRS_retVal_t Thresh_setVarsFile(char *vars, NVS_Handle fileHandle, char * fileCache)
//{
//
//    char *fileContent = CRS_malloc(1200);
//    char *prevFileContnet = fileContent;
//    NVS_Attrs fileRegionAttrs;
//    NVS_getAttrs(fileHandle, &fileRegionAttrs);
//    if (fileContent == NULL)
//    {
//        return CRS_FAILURE;
//    }
//    memset(fileContent, 0, 1200);
//    char strlenStr[STRLEN_BYTES] = { 0 };
//
//    NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize,
//             (void*) fileContent, MAX_LINE_CHARS);
//    memcpy(strlenStr, fileContent, STRLEN_BYTES);
//    uint32_t strlenPrev;
//    if (strlen(strlenStr) > 3)
//    {
//        CRS_free(fileContent);
//
//        return CRS_FAILURE;
//    }
//    if (strlenStr[0] == '\0')
//    {
//        strlenPrev = 0;
//    }
//    else
//    {
//        strlenPrev = CLI_convertStrUint(strlenStr);
//    }
//    if (strlenPrev > MAX_LINE_CHARS)
//    {
//        CRS_free(fileContent);
//
//        return CRS_FAILURE;
//    }
//    if (strlenPrev > MAX_LINE_CHARS)
//    {
//        NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
//                 (void*) fileContent + 512, 512);
//    }
//
//    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
//    char copyFile[MAX_LINE_CHARS] = { 0 };
//
//    const char s[2] = " ";
//
//    Vars_setVars(copyFile, vars, s);
//    const char d[2] = "\n";
//
//    Vars_setVars(copyFile, copyFileContent, d);
//
//    memset(fileContent, 0, 1100);
//    uint32_t newStrlen = strlen(copyFile);
//    memset(strlenStr, 0, STRLEN_BYTES);
//    int2hex(newStrlen, strlenStr);
//
//    memcpy(fileContent, strlenStr, STRLEN_BYTES);
//    memcpy(fileContent + STRLEN_BYTES + 1, copyFile, newStrlen);
//    size_t startFile = ((fileIndex) * gRegionAttrs.sectorSize);
//    NVS_write(gNvsHandle, startFile, (void*) fileContent,
//    STRLEN_BYTES + newStrlen + 3,
//              NVS_WRITE_ERASE);
//
//    fileContent = fileContent + STRLEN_BYTES + 1;
//    char *ptr;
//    if (fileIndex)
//    {
//        ptr = gThreshCache;
//    }
//    else
//    {
//        ptr = gEnvCache;
//    }
//
//    if (ptr == NULL)
//    {
//        ptr = CRS_malloc(strlen(fileContent) + 100);
//        if (ptr == NULL)
//        {
//            CRS_free(prevFileContnet);
//
//            return CRS_FAILURE;
//        }
//        memset(ptr, 0, strlen(fileContent) + 100);
//        memcpy(ptr, fileContent, strlen(fileContent));
//    }
//    else
//    {
//        CRS_free(ptr);
//        ptr = CRS_malloc(strlen(fileContent) + 100);
//        if (ptr == NULL)
//        {
//            CRS_free(prevFileContnet);
//
//            return CRS_FAILURE;
//        }
//        memset(ptr, 0, strlen(fileContent) + 100);
//        memcpy(ptr, fileContent, strlen(fileContent));
//    }
//
////    if(fileIndex){
////        gThreshCache = gTempCache;
////    }
////    else{
////        gEnvCache = gTempCache;
////    }
////    gTempCache = NULL;
//    if (fileIndex)
//    {
//        gThreshCache = ptr;
//    }
//    else
//    {
//        gEnvCache = ptr;
//    }
//    CRS_free(prevFileContnet);
//
//    return CRS_SUCCESS;
//}
//
//CRS_retVal_t Thresh_rmVarsFile(char *vars, NVS_Handle fileHandle, char * fileCache)
//{
//
//    char *fileContent = CRS_malloc(1200);
//    char *prevFileContnet = fileContent;
//    NVS_Attrs fileRegionAttrs;
//    NVS_getAttrs(fileHandle, &fileRegionAttrs);
//    if (fileContent == NULL)
//    {
//        return CRS_FAILURE;
//    }
//    memset(fileContent, 0, 1200);
//    char strlenStr[STRLEN_BYTES] = { 0 };
//
//    NVS_read(gNvsHandle, fileIndex * gRegionAttrs.sectorSize,
//             (void*) fileContent, MAX_LINE_CHARS);
//    memcpy(strlenStr, fileContent, STRLEN_BYTES);
//    uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
//    if (strlen(strlenStr) > 3)
//    {
//        CRS_free(fileContent);
//
//        return CRS_FAILURE;
//    }
//    if (strlenPrev > MAX_LINE_CHARS)
//    {
//        CRS_free(fileContent);
//
//        return CRS_FAILURE;
//    }
//    if (strlenPrev > MAX_LINE_CHARS)
//    {
//
//        NVS_read(gNvsHandle, (fileIndex) * gRegionAttrs.sectorSize + 512,
//                 (void*) fileContent + 512, 512);
//    }
//
//    char *copyFileContent = fileContent + STRLEN_BYTES + 1;
//    char copyFile[1148] = { 0 };
//
//    const char s[2] = " ";
//    char *key;
//    char *delim;
//    char *key_indx;
//    char *token = strtok(vars, s);
//    char *tok_cont = token + strlen(token) + 1;
//    while (token)
//    {
//        key = strtok(copyFileContent, "\n");
//        while (key)
//        {
//            delim = strstr(key, "=");
//            if (delim)
//            {
//                *delim = '\0';
//                if (strcmp(key, token) == 0)
//                {
//                    *delim = '=';
//                    memset(key, 255, strlen(key));
//                    key_indx = key;
//                    key = NULL;
//                }
//                else
//                {
//                    *delim = '=';
//                    key_indx = key;
//                    key = strtok(NULL, "\n");
//                }
//            }
//            else
//            {
//                key_indx = key;
//                key = strtok(NULL, "\n");
//            }
//            *(key_indx + strlen(key_indx)) = '\n';
//        }
//        if (tok_cont < vars + MAX_LINE_CHARS)
//        {
//            token = strtok(tok_cont, s);
//            tok_cont = token + strlen(token) + 1;
//        }
//        else
//        {
//            token = NULL;
//        }
//    }
//    const char d[2] = "\n";
//    token = strtok(copyFileContent, d);
//    while (token != NULL)
//    {
//        if (strstr(token, "="))
//        {
//            strcat(copyFile, token);
//            strcat(copyFile, "\n");
//        }
//        token = strtok(NULL, d);
//
//    }
//
//    memset(fileContent, 0, 1100);
//    uint32_t newStrlen = strlen(copyFile);
//    memset(strlenStr, 0, STRLEN_BYTES);
//    int2hex(newStrlen, strlenStr);
//
//    memcpy(fileContent, strlenStr, STRLEN_BYTES);
//    memcpy(fileContent + STRLEN_BYTES + 1, copyFile, newStrlen);
//    size_t startFile = ((fileIndex) * gRegionAttrs.sectorSize);
//    NVS_write(gNvsHandle, startFile, (void*) fileContent,
//    STRLEN_BYTES + newStrlen + 3,
//              NVS_WRITE_ERASE);
//
//    fileContent = fileContent + STRLEN_BYTES + 1;
//    char *ptr;
//    if (fileIndex)
//    {
//        ptr = gThreshCache;
//    }
//    else
//    {
//        ptr = gEnvCache;
//    }
//
//    if (ptr == NULL)
//    {
//        ptr = CRS_malloc(strlen(fileContent) + 100);
//        if (ptr == NULL)
//        {
//            CRS_free(prevFileContnet);
//
//            return CRS_FAILURE;
//        }
//        memset(ptr, 0, strlen(fileContent) + 100);
//        memcpy(ptr, fileContent, strlen(fileContent));
//    }
//    else
//    {
//        CRS_free(ptr);
//        ptr = CRS_malloc(strlen(fileContent) + 100);
//        if (ptr == NULL)
//        {
//            CRS_free(prevFileContnet);
//
//            return CRS_FAILURE;
//        }
//        memset(ptr, 0, strlen(fileContent) + 100);
//        memcpy(ptr, fileContent, strlen(fileContent));
//    }
//
////    if(fileIndex){
////        CRS_free(gThreshCache);
////        gThreshCache = gTempCache;
////    }
////    else{
////        CRS_free(gEnvCache);
////        gEnvCache = gTempCache;
////    }
//    if (fileIndex)
//    {
//        gThreshCache = ptr;
//    }
//    else
//    {
//        gEnvCache = ptr;
//    }
//    CRS_free(prevFileContnet);
//
//    return CRS_SUCCESS;
//}

/******************************************************************************
 Local Functions
 *****************************************************************************/

void Vars_setVars(char *file, char *vars, const char *d)
{

    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array
    const char s[2] = "\n";

    char *key;
    char *delim;
    char copyFile[2048];
    strcpy(copyFile, file);
    char copyVars[2048];
    strcpy(copyVars, vars);
    char fileContent[2048] = {0};
    char *token = strtok(copyVars, " ");
    bool isBefore = false;


    char *keyDelim;
    bool isAfter = false;
    while (token != NULL)
    {
        key = strtok(NULL, " ");
        while (key)
        {
            delim = strstr(token, "=");
            keyDelim = strstr(key, "=");
            if (delim && keyDelim)
            {
                *delim = '\0';
                *keyDelim = '\0';
                if (strcmp(key, token) == 0)
                {
                    isAfter = true;
                }
                *delim = '=';
                *keyDelim = '=';
            }
            key = strtok(NULL, " ");
        }
        if(!isAfter){
            strcat(fileContent, token);
            strcat(fileContent, s);
        }
        isAfter = false;
        strcpy(copyVars, vars);
        key = strtok(copyVars, " ");
        while (key != token)
        {
            key = strtok(NULL, " ");
        }
        token = strtok(NULL, " ");

    }

    strcpy(copyVars, vars);
    token = strtok(copyFile, s);
    while (token != NULL)
    {
        key = strtok(copyVars, " ");
        while (key)
        {
            delim = strstr(token, "=");
            keyDelim = strstr(key, "=");
            if (delim && keyDelim)
            {
                *delim = '\0';
                *keyDelim = '\0';
                if (strcmp(key, token) == 0)
                {
                    isBefore = true;
                }
                *delim = '=';
                *keyDelim = '=';
            }
            key = strtok(NULL, " ");
        }
        if(!isBefore){
            strcat(fileContent, token);
            strcat(fileContent, s);
        }
        isBefore = false;
        strcpy(copyFile, file);
        strcpy(copyVars, vars);
        key = strtok(copyFile, s);
        while (strcmp(key, token) != 0)
        {
            key = strtok(NULL, s);
        }
        token = strtok(NULL, s);

    }

    memcpy(file, fileContent, 2048);
}

void Vars_getVars(char *file, char *keys, char *values)
{
    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array
    const char s[2] = " ";

//    memset(values, 0, MAX_LINE_CHARS);
    char *key;
    char *delim;
    //char copyFile[strlen(file)+1];
    char copyFile[2048];
    strcpy(copyFile, file);
    //char copyKeys[strlen(keys)+1];
    char copyKeys[2048];
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
        while (strcmp(key, token) != 0)
        {
            key = strtok(NULL, s);
        }
        token = strtok(NULL, s);

    }
}

void Vars_removeVars(char *file, char *keys){
    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array
    const char s[2] = "\n";

    char *key;
    char *delim;
    char copyFile[2048];
    strcpy(copyFile, file);
    char copyKeys[2048];
    strcpy(copyKeys, keys);
    char fileContent[2048] = {0};
    char *token = strtok(copyFile, s);
    bool delete = false;
    while (token != NULL)
    {
        key = strtok(copyKeys, " ");
        while (key)
        {
            delim = strstr(token, "=");
            if (delim)
            {
                *delim = '\0';
                if (strcmp(key, token) == 0)
                {
                    delete = true;
                }
                *delim = '=';
            }
            key = strtok(NULL, " ");
        }
        if(!delete){
            strcat(fileContent, token);
            strcat(fileContent, s);
        }
        delete = false;
        strcpy(copyFile, file);
        strcpy(copyKeys, keys);
        key = strtok(copyFile, s);
        while (strcmp(key, token) != 0)
        {
            key = strtok(NULL, s);
        }
        token = strtok(NULL, s);

    }
    memcpy(file, fileContent, 2048);
}

bool Vars_createFile(NVS_Handle * fileHandle){
    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    int_fast16_t retStatus = NVS_erase(*fileHandle, 0, fileRegionAttrs.sectorSize);
    if(retStatus != NVS_STATUS_SUCCESS){
        return false;
    }
    // zero first 32 header bytes + first byte of file
    char header [STRLEN_BYTES+1] = {0};
    // add the header signature to fifth byte location
    strcat(&header[4], VAR_HEADER);
    //char * header = CRS_calloc(sizeof(VAR_HEADER), sizeof(char));
    //memcpy(header, VAR_HEADER, sizeof(VAR_HEADER));

    retStatus = NVS_write(*fileHandle, 0, (void*) header, STRLEN_BYTES+1, NVS_WRITE_ERASE);
    //CRS_free(header);
    if(retStatus != NVS_STATUS_SUCCESS){
        return false;
    }
    return true;
}


int Vars_getLength(NVS_Handle * fileHandle){

    // NVS_Attrs fileRegionAttrs;
    // NVS_getAttrs(fileHandle, &fileRegionAttrs);

    char fileHeader[STRLEN_BYTES] = { 0 };
    NVS_read(*fileHandle, 0,
            (void*) fileHeader, STRLEN_BYTES);

    uint32_t length = strtoul(fileHeader, NULL, 16);
    if(memcmp(&fileHeader[4], VAR_HEADER, sizeof(VAR_HEADER)) != 0 ){
        return -1;
    }

    return length + 1;
}

CRS_retVal_t Vars_getFile(NVS_Handle * fileHandle, char * file){

    uint32_t fileLength = Vars_getLength(fileHandle);
    // NVS_Attrs fileRegionAttrs;
    // NVS_getAttrs(fileHandle, &fileRegionAttrs);

    NVS_read(*fileHandle, STRLEN_BYTES,
             file, fileLength);

    return CRS_SUCCESS;
}

uint16_t Vars_setFileVars(NVS_Handle * fileHandle, char * file, char * vars){

    // char * fileCopy = CRS_malloc(maxFileSize);
    // memcpy(file, fileCopy, strlen(file));

    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    uint16_t length = strlen(file);

    // get new file string
    char copyFile[MAX_LINE_CHARS] = { 0 };
    const char s[2] = " ";
    Vars_setVars(file, vars, s);
//
//    const char d[2] = "\n";
//
//    Vars_setVars(copyFile, file, d);


    // get new file length and write it to flash
    uint16_t newLength = strlen(file);
    //memcpy(file, copyFile, newLength+1);

    //char fileLength [4] = {0};
    char fileHeader [STRLEN_BYTES] = {0};
    sprintf(fileHeader, "%x", newLength);
    strcat(&fileHeader[4], VAR_HEADER);
    // write new file to flash
//    if (newLength < length){
//        NVS_erase(*fileHandle, STRLEN_BYTES+newLength, length - newLength);
//        NVS_erase(*fileHandle, 0, 4);
//    }

    // this erases all of the sector
    NVS_write(*fileHandle, 0, fileHeader, STRLEN_BYTES, NVS_WRITE_ERASE);

    NVS_read(*fileHandle, 0, (void*) fileHeader, STRLEN_BYTES);

    char fileCopy [1024] = {0};
    NVS_write(*fileHandle, STRLEN_BYTES, file, newLength+1, NVS_WRITE_POST_VERIFY);
    NVS_read(*fileHandle, 0, (void*) fileCopy, 1024);

    return newLength+1;
}

uint16_t Vars_removeFileVars(NVS_Handle * fileHandle, char * file, char * vars){

    // char * fileCopy = CRS_malloc(maxFileSize);
    // memcpy(file, fileCopy, strlen(file));

    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    uint16_t length = strlen(file);
    // get new file string
    Vars_removeVars(file, vars);

    // get new file length and write it to flash
    uint16_t newLength = strlen(file);
    char fileHeader [STRLEN_BYTES] = {0};
    sprintf(fileHeader, "%x", newLength);
    strcat(&fileHeader[4], VAR_HEADER);
    NVS_write(*fileHandle, 0, fileHeader, STRLEN_BYTES, NVS_WRITE_ERASE);

//    // write new file to flash
//    if (newLength < length){
//        NVS_erase(*fileHandle, STRLEN_BYTES+newLength, length - newLength);
//    }

    NVS_write(*fileHandle, STRLEN_BYTES, file, newLength, NVS_WRITE_POST_VERIFY);

    return newLength+1;

}

