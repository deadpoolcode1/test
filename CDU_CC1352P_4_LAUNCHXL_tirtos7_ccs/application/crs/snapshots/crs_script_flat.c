/*
 * crs_script_flat.c
 *
 *  Created on: Nov 3, 2022
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <ti/sysbios/knl/Task.h> // TODO erase at the end

#include "application/crs/snapshots/crs_script_flat.h"
#include "application/crs/crs_nvs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define LINE_LENGTH 300
#define LINE_SEPARATOR  '\n'

typedef struct parsingStruct{
    char *fileBuffer;
    uint32_t idxOfFileBuffer;

}ScriptFlatparsingContainer_t;

enum command_type
{
    CmdType_NUM_OF_COMMAND_TYPES
};

typedef CRS_retVal_t (*Line_Handler_fnx) (ScriptFlatparsingContainer_t *, char*);

/******************************************************************************
 Local variables
 *****************************************************************************/
static Line_Handler_fnx gFunctionsTable[CmdType_NUM_OF_COMMAND_TYPES] =
{

};



/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t getNextLine(ScriptFlatparsingContainer_t *parsingContainer, char *next_line);
static CRS_retVal_t handleLine(ScriptFlatparsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t checkLineSyntax(ScriptFlatparsingContainer_t *parsingContainer, char *line, enum command_type cmd);




/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t scriptFlat_init(void)
{
    return CRS_SUCCESS;
}
CRS_retVal_t scriptFlat_runFile(uint8_t *filename)
{
    ScriptFlatparsingContainer_t parsingContainer = {0};
    parsingContainer.fileBuffer = Nvs_readFileWithMalloc((char*)filename);
    if (NULL == parsingContainer.fileBuffer)
    {
        CLI_cliPrintf("\r\n bad file name");
        CLI_cliPrintf("\r\nStatus 0x%x", CRS_FAILURE);
        return CRS_FAILURE;
    }

    uint32_t lenOfFlatFile = strlen(parsingContainer.fileBuffer);

    char line[LINE_LENGTH] = {0};

    do
    {
        memset(line, 0,sizeof(line));
        getNextLine(&parsingContainer,line);
        CLI_cliPrintf("\r\nline is %s",line);
        Task_sleep(5000);
//        retStatus = handleLine(&parsingContainer, line);
        if (CRS_SUCCESS != retStatus)
        {
            CLI_cliPrintf("\r\nSyntax Error");
            return CRS_FAILURE;
        }

    }while (parsingContainer.idxOfFileBuffer < lenOfFlatFile);

}




/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t getNextLine(ScriptFlatparsingContainer_t *parsingContainer, char *next_line)
{
    char *ptr = &parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer];
    uint32_t len = 0;
    while(*ptr != LINE_SEPARATOR && *ptr != 0)
    {
        ptr++;
        len++;
    }
    memcpy(next_line, &parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer],len);
    parsingContainer->idxOfFileBuffer += len + 1;

    return CRS_SUCCESS;
}

static CRS_retVal_t handleLine(ScriptFlatparsingContainer_t *parsingContainer, char *line)
{
    uint8_t i = 0;
    for (i = 0; i < CmdType_NUM_OF_COMMAND_TYPES; i++)
    {
       if(CRS_SUCCESS == gFunctionsTable[i](parsingContainer, line))
       {
           return CRS_SUCCESS;
       }
    }

    return CRS_FAILURE;
}
