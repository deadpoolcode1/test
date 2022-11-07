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
#define STRUCT_ITEM_LEN 50
#define PACKAGE_NUM_OF_FILES    10
#define DISCSEQ_NUM_OF_CMDS    10
#define DISCSEQ_COMMAND_LEN 2
#define INV_CMD_PARAMS_NUM 9
#define PACKAGE_CMD_PARAMS_NUM  2
#define DISCSEQ_CMD_PARAMS_NUM  2


#define LINE_SEPARATOR  '\n'
#define LABEL_INVENTORY "[INVENTORY]"
#define LABEL_PACKAGES "[PACKAGES]"
#define LABEL_DISCSEQ "[DISCSEQ]"
typedef enum fileType
{
    fileType_SNAPSHOT,
    fileType_SCRIPT,
    fileType_NUM_OF_TYPES
} fileType_t;

typedef enum flavor
{
    flavor_SPI_NATIVE,
    flavor_SPI_SLAVE,
    flavor_NUM_OF_TYPES
} flavorType_t;

typedef enum chipType
{
    chip_RF,
    chip_DIG,
    chip_NUM_OF_TYPES
} chipType_t;


typedef struct fileStruct{
    char fileName [STRUCT_ITEM_LEN];
    uint32_t lineNumber;
    fileType_t fileType;
    char parameters [STRUCT_ITEM_LEN];
}ScriptFlatFileContainer_t;

typedef struct discSeqCmdStruct{
    char command [STRUCT_ITEM_LEN];
    char expRead [STRUCT_ITEM_LEN];
}ScriptFlatDiscSeqFpgaCmdContainer_t;

typedef struct packageCmdStruct{
    int8_t packageId;
    ScriptFlatFileContainer_t files[PACKAGE_NUM_OF_FILES];
}ScriptFlatPackageCmd_t;


typedef struct discseqCmdStruct{
    int8_t discSeqId;
    ScriptFlatDiscSeqFpgaCmdContainer_t commands [DISCSEQ_NUM_OF_CMDS];
}ScriptFlatDiscseqCmd_t;

typedef struct inventoryCmdStruct{
    uint8_t cmdId;
    char chipName [STRUCT_ITEM_LEN];
    char fpgaCmd [STRUCT_ITEM_LEN];
    ScriptFlatDiscseqCmd_t discoverySeq;
    bool isMandatory;
    chipType_t chipType;
    flavorType_t flavor;
    ScriptFlatPackageCmd_t package;
}ScriptFlatInventoryCmd_t;


enum command_type
{
    CmdType_UNIT_NAME_LABEL,
    CmdType_INVENTORY_LABEL,
    CmdType_PACKAGES_LABEL,
    CmdType_DISCSEQ_LABEL,
    CmdType_INVENTORY_CMD,
    CmdType_PACKAGES_CMD,
    CmdType_DISCSEQ_CMD,
    CmdType_EMPTY_LINE,
    CmdType_SQUARE_BRACKETS,
    CmdType_NUM_OF_COMMAND_TYPES
};


typedef struct parsingStruct{
    char *fileBuffer;
    uint32_t idxOfFileBuffer;
    bool isInInventory;
    bool isInPackage;
    bool isInDiscseq;
    bool shouldIgnoreLine;
}ScriptFlatParsingContainer_t;
typedef CRS_retVal_t (*Line_Handler_fnx) (ScriptFlatParsingContainer_t *, char*);


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t getNextLine(ScriptFlatParsingContainer_t *parsingContainer, char *next_line);
static CRS_retVal_t handleLine(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t checkLineSyntax(ScriptFlatParsingContainer_t *parsingContainer, char *line, enum command_type cmd);
static bool isLineInBrackets(char *line);

// line handlers
static CRS_retVal_t unitNameLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t inventoryLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t packagesLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t discseqLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t inventoryCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t packageCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t discseqCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t emptyLineHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t emptyBracketsHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t ignoreLineHandler (ScriptFlatParsingContainer_t *parsingContainer, char *line);
/******************************************************************************
 Local variables
 *****************************************************************************/
static Line_Handler_fnx gFunctionsTable[CmdType_NUM_OF_COMMAND_TYPES] =
{
 unitNameLabelHandler,
 inventoryLabelHandler,
 packagesLabelHandler,
 discseqLabelHandler,
 inventoryCommandHandler,
 packageCommandHandler,
 discseqCommandHandler,
 emptyLineHandler,
 emptyBracketsHandler,
// ignoreLineHandler
};






/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t scriptFlat_init(void)
{
    return CRS_SUCCESS;
}
CRS_retVal_t scriptFlat_runFile(uint8_t *filename)
{
    ScriptFlatParsingContainer_t parsingContainer = {0};
    parsingContainer.fileBuffer = Nvs_readFileWithMalloc((char*)filename);
    if (NULL == parsingContainer.fileBuffer)
    {
        CLI_cliPrintf("\r\n bad file name");
        CLI_cliPrintf("\r\nStatus 0x%x", CRS_FAILURE);
        return CRS_FAILURE;
    }

    uint32_t lenOfFlatFile = strlen(parsingContainer.fileBuffer);

    char line[LINE_LENGTH] = {0};
    CRS_retVal_t retStatus = CRS_SUCCESS;
    do
    {
        memset(line, 0,sizeof(line));
        getNextLine(&parsingContainer,line);
        CLI_cliPrintf("\r\nline is %s",line);
        Task_sleep(5000);
        retStatus = handleLine(&parsingContainer, line);
        if (CRS_SUCCESS != retStatus)
        {
            CLI_cliPrintf("\r\nSyntax Error");
            return CRS_FAILURE;
        }

    }while (parsingContainer.idxOfFileBuffer < lenOfFlatFile);


    free (parsingContainer.fileBuffer);
    parsingContainer.fileBuffer = NULL;

    return CRS_SUCCESS;
}




/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t getNextLine(ScriptFlatParsingContainer_t *parsingContainer, char *next_line)
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

static CRS_retVal_t handleLine(ScriptFlatParsingContainer_t *parsingContainer, char *line)
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


static CRS_retVal_t unitNameLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_UNIT_NAME_LABEL))
    {
        return CRS_FAILURE;
    }

    parsingContainer->shouldIgnoreLine = true;

    return CRS_SUCCESS;
}

static CRS_retVal_t inventoryLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_INVENTORY_LABEL))
    {
        return CRS_FAILURE;
    }

    parsingContainer->isInInventory = true;
    parsingContainer->isInPackage = false;
    parsingContainer->isInDiscseq = false;

    return CRS_SUCCESS;
}
static CRS_retVal_t packagesLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_PACKAGES_LABEL))
    {
        return CRS_FAILURE;
    }

    parsingContainer->isInInventory = false;
    parsingContainer->isInPackage = true;
    parsingContainer->isInDiscseq = false;

    return CRS_SUCCESS;
}
static CRS_retVal_t discseqLabelHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_DISCSEQ_LABEL))
    {
        return CRS_FAILURE;
    }

    parsingContainer->isInInventory = false;
    parsingContainer->isInPackage = false;
    parsingContainer->isInDiscseq = true;

    return CRS_SUCCESS;
}
static CRS_retVal_t inventoryCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_INVENTORY_CMD))
    {
        return CRS_FAILURE;
    }

    if (parsingContainer->isInInventory == false)
    {
        CLI_cliPrintf("\r\nInventory command outside of inventory");
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}
static CRS_retVal_t packageCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_PACKAGES_CMD))
    {
        return CRS_FAILURE;
    }

    if (parsingContainer->isInPackage == false)
    {
        CLI_cliPrintf("\r\nPackage command outside of package");
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}
static CRS_retVal_t discseqCommandHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_DISCSEQ_CMD))
    {
        return CRS_FAILURE;
    }
    if (parsingContainer->isInDiscseq == false)
    {
        CLI_cliPrintf("\r\nDiscseq command outside of discseq");
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}
static CRS_retVal_t emptyLineHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_EMPTY_LINE))
    {
        return CRS_FAILURE;
    }

    // ignore

    return CRS_SUCCESS;
}

static CRS_retVal_t emptyBracketsHandler(ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_SQUARE_BRACKETS))
    {
        return CRS_FAILURE;
    }

    // ignore

    return CRS_SUCCESS;
}

static CRS_retVal_t checkLineSyntax(ScriptFlatParsingContainer_t *parsingContainer, char *line, enum command_type cmd)
{
    if (cmd == CmdType_UNIT_NAME_LABEL)
    {
        if (false == isLineInBrackets(line))
        {
            return CRS_FAILURE;
        }

        char *ptr = line;
        ptr ++; // skip '['
        char *unit_name = "CDU-";
        if (0 == memcmp(unit_name, ptr, strlen(unit_name)))
        {
            return CRS_SUCCESS;
        }

        unit_name = "CRU-";
        if (0 == memcmp(unit_name, ptr, strlen(unit_name)))
        {
            return CRS_SUCCESS;
        }

        return CRS_FAILURE;
    }

    else if (cmd == CmdType_INVENTORY_LABEL)
    {
        if (0 != memcmp(LABEL_INVENTORY, line, strlen(LABEL_INVENTORY)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_PACKAGES_LABEL)
    {
        if (0 != memcmp(LABEL_PACKAGES, line, strlen(LABEL_PACKAGES)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    else if (cmd == CmdType_DISCSEQ_LABEL)
    {
        if (0 != memcmp(LABEL_DISCSEQ, line, strlen(LABEL_DISCSEQ)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    else if (cmd == CmdType_INVENTORY_CMD)
    {
        if (false == parsingContainer->isInInventory)
        {
            return CRS_FAILURE;
        }
        char *ptr = line;
        uint8_t comma_counter = 0;
        if (*ptr == ',')
        {
            return CRS_FAILURE;
        }
        while(*ptr != 0 && *ptr != LINE_SEPARATOR)
        {
            if (*ptr == ',')
            {
                comma_counter++;
            }
        }

        if (*(ptr - 1) == ',' || comma_counter != INV_CMD_PARAMS_NUM)
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    else if (cmd == CmdType_DISCSEQ_CMD)
    {
        if (false == parsingContainer->isInDiscseq)
        {
            return CRS_FAILURE;
        }
        char *ptr = line;
        uint8_t comma_counter = 0;
        if (*ptr == ',')
        {
            return CRS_FAILURE;
        }
        while(*ptr != 0 && *ptr != LINE_SEPARATOR)
        {
            if (*ptr == ',')
            {
                comma_counter++;
            }
        }

        if (*(ptr - 1) == ',' || comma_counter != DISCSEQ_CMD_PARAMS_NUM)
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    else if (cmd == CmdType_PACKAGES_CMD)
    {
        if (false == parsingContainer->isInPackage)
        {
            return CRS_FAILURE;
        }
        char *ptr = line;
        uint8_t comma_counter = 0;
        if (*ptr == ',')
        {
            return CRS_FAILURE;
        }
        while(*ptr != 0 && *ptr != LINE_SEPARATOR)
        {
            if (*ptr == ',')
            {
                comma_counter++;
            }
        }

        if (*(ptr - 1) == ',' || comma_counter != PACKAGE_CMD_PARAMS_NUM)
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    else if (cmd == CmdType_EMPTY_LINE)
    {
        if(line[0] != 0)
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_SQUARE_BRACKETS)
    {
        if (false == isLineInBrackets(line))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }


    return CRS_FAILURE;
}

static bool isLineInBrackets(char *line)
{
    if (line[0] != '[')
    {
        return false;
    }

    char *ptr = line;
    while(ptr != NULL && *ptr != 0 && *ptr != LINE_SEPARATOR)
    {
        ptr++;
    }

    if (*(ptr - 1) == ']')
    {
        return true;
    }

    return false;
}

static CRS_retVal_t ignoreLineHandler (ScriptFlatParsingContainer_t *parsingContainer, char *line)
{
    if (parsingContainer->shouldIgnoreLine == true)
    {
        parsingContainer->shouldIgnoreLine = false;
        return CRS_SUCCESS;
    }

    return CRS_FAILURE;
}


