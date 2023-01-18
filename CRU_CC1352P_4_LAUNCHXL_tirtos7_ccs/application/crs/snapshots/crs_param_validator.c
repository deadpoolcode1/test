/*
 * crs_param_validator.c
 *
 *  Created on: Jan 10, 2023
 *      Author: yardenr
 */

/******************************************************************************
 Includes
 *****************************************************************************/

#include <stdlib.h> // atoi
#include <string.h> // strstr
#include "crs_param_validator.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define PARAM_LINE  "//@@ param "
#define NOT_FOUND   -1
#define LINE_TO_FIND_SIZE 50
#define NUM_OF_VALID_PARAM_VALS 30

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static int8_t getParamIdx(const CRS_nameValue_t nameValues[NAME_VALUES_SZ], const char *paramName);
static CRS_retVal_t getValidParamsArray(char *paramLine, int32_t validParams[NUM_OF_VALID_PARAM_VALS]);
static bool isValInArr(int32_t val, int32_t validParams[NUM_OF_VALID_PARAM_VALS]);
/******************************************************************************
 Public Functions
 *****************************************************************************/

bool isParamValid(const char *file, const CRS_nameValue_t nameValues[NAME_VALUES_SZ], const char *paramName)
{
    char lineToFind [LINE_TO_FIND_SIZE] = {0};
    memcpy(lineToFind,PARAM_LINE,strlen(PARAM_LINE));
    memcpy((lineToFind + strlen(PARAM_LINE)),paramName,strlen(paramName));

    char *paramLine = strstr(file,lineToFind);
    if (NULL == paramLine)
    {
        return true;    // no parameter restrictions
    }

    int32_t validParams[NUM_OF_VALID_PARAM_VALS] = {0};
    if (CRS_SUCCESS != getValidParamsArray(paramLine, validParams))
    {
        return true; // no parameter restrictions
    }

    int8_t paramIdx = getParamIdx(nameValues, paramName);
    if (NOT_FOUND == paramIdx)
    {
        // check if default param is valid
        char lineCpy [LINE_TO_FIND_SIZE] = {0};
        memcpy(lineCpy,lineToFind, LINE_TO_FIND_SIZE);
        const char *seperator = ",";
        char *defaultParamStr = strtok(lineCpy,seperator); // //@@ param _gain
        defaultParamStr = strtok(NULL, seperator);// 4
        int32_t paramVal = atoi(defaultParamStr);
        return isValInArr(paramVal, validParams);
    }

    CRS_nameValue_t param = nameValues[paramIdx];
    int32_t paramVal = param.value;
    return isValInArr(paramVal, validParams);

}




/******************************************************************************
 Local Functions
 *****************************************************************************/
static int8_t getParamIdx(const CRS_nameValue_t nameValues[NAME_VALUES_SZ], const char *paramName)
{
    int8_t i = 0;
    for (i = 0; i < NAME_VALUES_SZ; i++)
    {
        if (0 == memcmp(nameValues[i].name, paramName, strlen(paramName)))
        {
            return i;
        }
    }

    return NOT_FOUND;
}

static CRS_retVal_t getValidParamsArray(char *paramLine, int32_t validParams[NUM_OF_VALID_PARAM_VALS])
{
    char *arrStart = strstr(paramLine, "[");
    char *arrEnd = strstr(paramLine, "]");
    if (NULL == arrStart || NULL == arrEnd || arrStart+1 == arrEnd) // if empty []
    {
        return CRS_FAILURE; // no param restrictions
    }
    char *arrIterator = arrStart;
    arrIterator ++; // go to first element in array
    uint8_t i = 0;
    while(arrIterator < arrEnd)
    {
        validParams[i] = atoi(arrIterator);
        i++;
        arrIterator = strstr(arrIterator, ","); // go to nearest ,
        if (NULL == arrIterator)
        {
            break;
        }
        arrIterator ++; // go to next element in array
    }

    return CRS_SUCCESS;
}

static bool isValInArr(int32_t val, int32_t validParams[NUM_OF_VALID_PARAM_VALS])
{
    uint8_t i = 0;
    for (i = 0; i < NUM_OF_VALID_PARAM_VALS; i++)
    {
        if (val == validParams[i])
        {
            return true;
        }
    }

    return false;
}
