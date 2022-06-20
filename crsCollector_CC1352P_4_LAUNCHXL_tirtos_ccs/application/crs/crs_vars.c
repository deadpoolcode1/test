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
 Local Function Prototypes
 *****************************************************************************/

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Vars_setVars(char *file, char * vars, const char *d, char* returnedFile){

    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array

    const char s[2] = "\n";
    char *key;
    char *delim;

    char * copyFile = CRS_calloc(strlen(file)+1, sizeof(char));
    strcpy(copyFile, file);

    char * copyVars = CRS_calloc(strlen(vars)+1, sizeof(char) );
    strcpy(copyVars, vars);

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
        if(!isAfter && strstr(token, "=")){
            strcat(returnedFile, token);
            strcat(returnedFile, s);
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
            strcat(returnedFile, token);
            strcat(returnedFile, s);
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

//    memcpy(file, fileContent, 2048);
    CRS_free(copyFile);
    CRS_free(copyVars);
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

void Vars_removeVars(char *file, char *keys, char * returnedFile){
    // file: key=value\nkey=value\n
    // keys: key1 key2 key3
    // values: values [MAX_LINE_CHARS];
    // return key=value to values array

    char * copyFile = CRS_calloc(strlen(file)+1, sizeof(char));
    strcpy(copyFile, file);

    char * copyKeys = CRS_calloc(strlen(keys)+1, sizeof(char));
    strcpy(copyKeys, keys);

    char *key;
    char *delim;
    char *token = strtok(copyFile, "\n");
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
            strcat(returnedFile, token);
            strcat(returnedFile, "\n");
        }
        delete = false;
        strcpy(copyFile, file);
        strcpy(copyKeys, keys);
        key = strtok(copyFile, "\n");
        while (strcmp(key, token) != 0)
        {
            key = strtok(NULL, "\n");
        }
        token = strtok(NULL, "\n");

    }
    CRS_free(copyFile);
    CRS_free(copyKeys);
//    memcpy(file, fileContent, 2048);
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

CRS_retVal_t Vars_setFile(NVS_Handle * fileHandle, char * file){

    uint32_t fileLength = strlen(file);
//    NVS_Attrs fileRegionAttrs;
//    NVS_getAttrs(fileHandle, &fileRegionAttrs);

    char * fileCopy = CRS_calloc(fileLength+STRLEN_BYTES+1, sizeof(char));

    sprintf(fileCopy, "%x", fileLength);
    // add the header signature to fifth byte location
    strcat(&fileCopy[4], VAR_HEADER);
    memcpy(&fileCopy[STRLEN_BYTES], file, fileLength+1);

    int_fast16_t retStatus = NVS_write(*fileHandle, 0, (void*) fileCopy, fileLength+STRLEN_BYTES+1, NVS_WRITE_ERASE);
    CRS_free(fileCopy);
    if(retStatus == NVS_STATUS_SUCCESS){
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

uint16_t Vars_setFileVars(NVS_Handle * fileHandle, char * file, char * vars){

    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    uint16_t length = strlen(file);

    char * returnedFile = CRS_calloc(fileRegionAttrs.regionSize, sizeof(char));
//    memcpy(returnedFile, file, strlen(file)+1);

    // get new file string
    const char s[2] = " ";
    Vars_setVars(file, vars, s, returnedFile);

    // get new file length and write it to flash
    uint16_t newLength = strlen(returnedFile);
    memcpy(file, returnedFile, newLength+1);

    //char fileLength [4] = {0};
    char fileHeader [STRLEN_BYTES] = {0};
    sprintf(fileHeader, "%x", newLength);
    strcat(&fileHeader[4], VAR_HEADER);

    // this erases all of the sector
    NVS_write(*fileHandle, 0, fileHeader, STRLEN_BYTES, NVS_WRITE_ERASE);

    NVS_read(*fileHandle, 0, (void*) fileHeader, STRLEN_BYTES);

//    char fileCopy [1024] = {0};
    NVS_write(*fileHandle, STRLEN_BYTES, file, newLength+1, NVS_WRITE_POST_VERIFY);
//    NVS_read(*fileHandle, 0, (void*) fileCopy, 1024);

    CRS_free(returnedFile);
    return newLength+1;
}

uint16_t Vars_removeFileVars(NVS_Handle * fileHandle, char * file, char * vars){

    // char * fileCopy = CRS_malloc(maxFileSize);
    // memcpy(file, fileCopy, strlen(file));

    NVS_Attrs fileRegionAttrs;
    NVS_getAttrs(*fileHandle, &fileRegionAttrs);
    uint16_t length = strlen(file);

    char * returnedFile = CRS_calloc(fileRegionAttrs.regionSize, sizeof(char));
//    memcpy(returnedFile, file, strlen(file)+1);

    // get new file string
    Vars_removeVars(file, vars, returnedFile);

    // get new file length and write it to flash
    uint16_t newLength = strlen(file);

    memcpy(file, returnedFile, newLength+1);


    char fileHeader [STRLEN_BYTES] = {0};
    sprintf(fileHeader, "%x", newLength);
    strcat(&fileHeader[4], VAR_HEADER);
    NVS_write(*fileHandle, 0, fileHeader, STRLEN_BYTES, NVS_WRITE_ERASE);

//    // write new file to flash
//    if (newLength < length){
//        NVS_erase(*fileHandle, STRLEN_BYTES+newLength, length - newLength);
//    }

    NVS_write(*fileHandle, STRLEN_BYTES, file, newLength+1, NVS_WRITE_POST_VERIFY);
    CRS_free(returnedFile);
    return newLength+1;

}

