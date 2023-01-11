/*
 * crs_script_returnvalues.c
 *
 *  Created on: Jan 11, 2023
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h> // str
#include "crs_script_returnvalues.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
enum
{
    STATUS_ELEMENT,
    NUM_OF_ELEMENTS
};


typedef struct scriptRetValElement
{
    char key [RETVAL_ELEMENT_KEY_SZ];
    char val [RETVAL_ELEMENT_VAL_SZ];
}scriptRetValElement_t;

typedef struct scriptRetVal
{
    scriptRetValElement_t elements[NUM_OF_ELEMENTS];
}scriptRetVal_t;


/******************************************************************************
 Global Variables
 *****************************************************************************/
static char *gScriptRetVals_statusMessages [] = {
                                          "OK",
                                          "General Failure",
                                          "Param out of range",
                                          "Invalid script line"
                                        };
static scriptRetVal_t gRetVal = {0};
static bool isModuleInit = false;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/



/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t ScriptRetVals_init(void)
{
//    if (NULL != gRetVal)
//    {
//        CRS_free(&gRetVal);
//    }
//    gRetVal = CRS_malloc(sizeof(gScriptRetVals_statusMessages[OK]));
//    if (NULL == gRetVal)
//    {
//        return CRS_FAILURE;
//    }
//    memset(gRetVal, 0, sizeof(gScriptRetVals_statusMessages[OK]));
//
//    memcpy(gRetVal, gScriptRetVals_statusMessages[OK], strlen(gScriptRetVals_statusMessages[OK]));
//    return CRS_SUCCESS;
    memset(&gRetVal, 0, sizeof(scriptRetVal_t));
    memcpy(gRetVal.elements[STATUS_ELEMENT].key, "status", strlen("status"));
    memcpy(gRetVal.elements[STATUS_ELEMENT].val, gScriptRetVals_statusMessages[scriptRetVal_OK], strlen(gScriptRetVals_statusMessages[scriptRetVal_OK]));

    isModuleInit = true;
    return CRS_SUCCESS;

}

CRS_retVal_t ScriptRetVals_getValue(char *key, char *value)
{
    if (!isModuleInit)
    {
        return CRS_FAILURE;
    }

    uint8_t i = 0;
    for(i = 0; i < NUM_OF_ELEMENTS; i++)
    {
        if(0 == memcmp(gRetVal.elements[i].key, key, strlen(key)))
        {
            memcpy(value, gRetVal.elements[i].val, strlen(gRetVal.elements[i].val));
            return CRS_SUCCESS;
        }
    }

    return CRS_FAILURE;
}

CRS_retVal_t ScriptRetVals_setValue(char *key, char* value)
{
    if (!isModuleInit)
    {
        return CRS_FAILURE;
    }
//
//    char *returnedFile = CRS_calloc(0,strlen(gRetVal) + strlen(vars));
//    if (NULL == returnedFile)
//    {
//        return CRS_FAILURE;
//    }
//
//    Vars_setVars(gRetVal, vars, NULL, returnedFile);
//    memset(gRetVal,0,strlen(gRetVal));
//    gRetVal = CRS_realloc(gRetVal, strlen(returnedFile));
//    if (NULL == gRetVal)
//    {
//        CRS_free(&returnedFile);
//        return CRS_FAILURE;
//    }
//
//    memcpy(gRetVal, returnedFile, strlen(returnedFile));
//    CRS_free(&returnedFile);
//
//    return CRS_SUCCESS;
    uint8_t i = 0;
    for(i = 0; i < NUM_OF_ELEMENTS; i++)
    {
        if(0 == memcmp(gRetVal.elements[i].key, key, strlen(key)))
        {
            memset(gRetVal.elements[i].val, 0, RETVAL_ELEMENT_VAL_SZ);
            memcpy(gRetVal.elements[i].val, value, strlen(value));
            return CRS_SUCCESS;
        }
    }

    return CRS_FAILURE;

}

CRS_retVal_t ScriptRetVals_setStatus(scriptReturnValue_t status)
{
//    char * statusMsg = gScriptRetVals_statusMessages[status];
//    char *vars = CRS_malloc(strlen(START_OF_STATUS)+strlen(statusMsg) + 1);
//    if (NULL == vars)
//    {
//        return CRS_FAILURE;
//    }
//    memcpy(vars,START_OF_STATUS,strlen(START_OF_STATUS));
//    memcpy(vars + strlen(START_OF_STATUS),statusMsg,strlen(statusMsg));
//
//    vars[strlen(vars) - 1] = '\n';
//
//    if (CRS_SUCCESS != ScriptRetVals_setValues(vars))
//    {
//        CRS_free(&vars);
//        return CRS_FAILURE;
//    }
//    CRS_free(&vars);

    char *statusMsg = gScriptRetVals_statusMessages[status];

    return ScriptRetVals_setValue(gRetVal.elements[STATUS_ELEMENT].key, statusMsg);
}

/******************************************************************************
 Local Functions
 *****************************************************************************/
