/*
 * crs_yarden.c
 *
 *  Created on: Oct 24, 2022
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_yarden.h"
#include "crs_cli.h"
#include "crs_nvs.h"
#include <ti/sysbios/knl/Task.h> // TODO erase at the end

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define CMD_IF  "if "
#define CMD_GOTO  "goto "
#define CMD_COMMENT  "// "
#define CMD_PARAM  "//@@ "
#define CMD_W  "w "
#define CMD_R  "r "
#define CMD_EW  "ew "
#define CMD_ER  "er "
#define CMD_APPLY  "apply"

#define LINE_SEPARTOR   '\n'
#define DECIMAL 10
#define NOT_FOUND   -1
#define ADDRESS_CONVERT_RATIO   4
#define LUT_REG_NUM 8
#define NUM_GLOBAL_REG 4
#define NUM_LUTS 4

#define GLOBAL_ADDR_START   0x3F
#define GLOBAL_ADDR_FINAL   0x42


enum command_type
{
    CmdType_IF, //if
    CmdType_GOTO,   //goto
    CmdType_COMMENT,    // "//"
    CmdType_PARAMS, // "//@@"
    CmdType_W, // w
    CmdType_R, // r
    CmdType_EW,   // ew
    CmdType_ER, // er
    CmdType_APPLY,  // apply
    CmdType_LABEL,   // "label:"
    CmdType_NUM_OF_COMMAND_TYPES
};

typedef struct Label_index
{
    char name [YARDEN_NAME_VALUES_SZ];
    char *label_start;
} Yarden_label_index_t;


typedef struct wrStruct
{
    char mode[5]; //w r ew er
    char addr[15]; //1a10601c
    char val [20]; // 0003
} Yarden_wrContainer_t;

typedef CRS_retVal_t (*Line_Handler_fnx) (char*);
static CRS_retVal_t getNextLine(char *next_line);
static CRS_retVal_t handleLine(char *line);
static void freeGFileBuffer(void);
static CRS_retVal_t checkLineSyntax(char *line, enum command_type cmd);
static char *myStrTok(char *srcString,const char *delim);
static uint8_t isDelim(char c, const char *delim);

static CRS_retVal_t gotoGivenLabel(char *label);

/* this function returns the index of given param in
 * global array,
 * if not found in array return NOT_FOUND (-1)
 */
static int8_t getParamIdx(char *param);
//static bool isGlobal(char *line);
static bool isGlobal(Yarden_wrContainer_t *wrContainer);

//static uint32_t getAddress(char *line);
//static CRS_retVal_t getVal(char *line, char *ret);
//static bool isInvalidAddr(char *line);
static bool isInvalidAddr(Yarden_wrContainer_t *wrContainer);

//static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber);
static CRS_retVal_t getLutNumberFromWRContainer(Yarden_wrContainer_t *wrContainer, uint32_t *lutNumber);
static CRS_retVal_t getLutRegFromWRContainer(Yarden_wrContainer_t *wrContainer, uint32_t *lutReg);

//static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg);
static CRS_retVal_t paramInit();
static CRS_retVal_t saveNameVals(CRS_nameValue_t nameVals[YARDEN_NAME_VALUES_SZ]);
static bool isStarValue(char *val);
static CRS_retVal_t handleStarLut(uint32_t lutNumber, uint32_t lutReg, char *val);
static CRS_retVal_t handleStarGlobal(uint32_t addrVal, char *val);
static CRS_retVal_t getStarValue(char *val, uint32_t *regVal);
static CRS_retVal_t initwrContainer(char *line, Yarden_wrContainer_t *ret);
// Command handlers
static CRS_retVal_t ifCommandHandler (char *line);
static CRS_retVal_t gotoCommandHandler (char *line);
static CRS_retVal_t commentCommandHandler (char *line);
static CRS_retVal_t paramsCommandHandler (char *line);
static CRS_retVal_t wCommandHandler (char *line);
static CRS_retVal_t rCommandHandler (char *line);
static CRS_retVal_t ewCommandHandler (char *line);
static CRS_retVal_t erCommandHandler (char *line);
static CRS_retVal_t applyCommandHandler (char *line);
static CRS_retVal_t labelCommandHandler (char *line);





/******************************************************************************
 Local variables
 *****************************************************************************/
static Line_Handler_fnx gFunctionsTable[CmdType_NUM_OF_COMMAND_TYPES] =
{
 ifCommandHandler,
 gotoCommandHandler,
 commentCommandHandler,
 paramsCommandHandler,
 wCommandHandler,
 rCommandHandler,
 ewCommandHandler,
 erCommandHandler,
 applyCommandHandler,
 labelCommandHandler
};

static char *gFileBuffer = NULL;
static uint32_t gIdxOfFileBuffer = 0;
static CRS_nameValue_t gNameValues[YARDEN_NAME_VALUES_SZ] = {0};
static uint8_t gNameValuesIdx = 0;
static uint16_t gGlobalReg[NUM_GLOBAL_REG] = { 0 };
static uint16_t gLineMatrix[NUM_LUTS][LUT_REG_NUM] = { 0 };


/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Yarden_init(void)
{
    return CRS_SUCCESS;
}

CRS_retVal_t Yarden_runFile(uint8_t *filename, CRS_nameValue_t nameVals[YARDEN_NAME_VALUES_SZ])
{
    paramInit();
    saveNameVals(nameVals);

    // read the file using nvs
    gFileBuffer = Nvs_readFileWithMalloc((char*)filename);
    if (NULL == gFileBuffer)
    {
        return CRS_FAILURE;
    }

    uint32_t lenOfFile = strlen(gFileBuffer);

    char line[300] = {0};

    CRS_retVal_t retStatus = CRS_SUCCESS;
    do
    {
        // each line needs to get handled/translated and sent to FPGA
        memset(line, 0,sizeof(line));
        getNextLine(line);
        CLI_cliPrintf("\r\nline is %s",line);
        Task_sleep(1000);
        retStatus = handleLine(line);
        if (CRS_SUCCESS != retStatus)
        {
//            do nothing
        }
    }while (gIdxOfFileBuffer < lenOfFile);

    freeGFileBuffer();
    return retStatus;

}

/******************************************************************************
 Private Functions
 *****************************************************************************/
static CRS_retVal_t getNextLine(char *next_line)
{
    char *ptr = &gFileBuffer[gIdxOfFileBuffer];
    uint32_t len = 0;
    while(*ptr != LINE_SEPARTOR && *ptr != 0)
    {
        ptr++;
        len++;
    }
    memcpy(next_line, &gFileBuffer[gIdxOfFileBuffer],len);
    gIdxOfFileBuffer += len + 1;

    return CRS_SUCCESS;
}

static void freeGFileBuffer(void)
{
    free (gFileBuffer);
    gFileBuffer = NULL;

}


static CRS_retVal_t handleLine(char *line)
{
    uint8_t i = 0;
    for (i = 0; i < CmdType_NUM_OF_COMMAND_TYPES; i++)
    {
        if(CRS_SUCCESS == gFunctionsTable[i](line))
        {
            return CRS_SUCCESS;
        }
    }

    return CRS_FAILURE;
}

//if param == comparedVal then label
static CRS_retVal_t ifCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_IF))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling if command");
    char *param = NULL;
    char *comparedVal = NULL;
    char label[20] = { 0 };
    const char sep [] = " ";

    int32_t comparedValInt = 0;
    char *ptr = line;

    ptr += strlen(CMD_IF); // skip "if "

    param = myStrTok(ptr, sep); // get param and skip "param "
    CLI_cliPrintf("parameter is %s",param);

    myStrTok(NULL, sep); // skip ==
    comparedVal = myStrTok(NULL, sep); // get comparedVal and skip "comparedVal "

    int8_t idx = getParamIdx(param);
    if (idx == NOT_FOUND)
    {
        return CRS_FAILURE;
    }

    comparedValInt = strtol(comparedVal, NULL, DECIMAL);
    if(gNameValues[idx].value != comparedValInt) // if == is false
    {
        return CRS_SUCCESS;
    }

    myStrTok(NULL, sep); // skip "then"
    ptr = myStrTok(NULL, sep);
    strcat(label, CMD_GOTO);
    strcat(label, ptr); // label says: "goto 'label'"
    gotoCommandHandler(label);// run goto command on this

    return CRS_SUCCESS;
}

static CRS_retVal_t gotoCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_GOTO))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling goto command");
    const char sep [] = " ";
    char *token = myStrTok (line, sep); // goto
    token = myStrTok(NULL, sep);// label
    CLI_cliPrintf("the label is %s",token);
    gotoGivenLabel(token);
    return CRS_SUCCESS;
}

static CRS_retVal_t commentCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_COMMENT))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling comment command");


    return CRS_SUCCESS;
}

static CRS_retVal_t paramsCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_PARAMS))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling params command");

    // move after //@@_
    line += strlen(CMD_PARAM);
    line ++;

    const char param_keyword [] = "param";
    uint8_t size = 5;
    if (0 != memcmp(line, param_keyword,size))
    {
        return CRS_FAILURE;
    }

    // move after 'param' keyword
    line += size;
    line ++;
    char sep [] = ",";
    char *varName = myStrTok(line, sep);

    int8_t idx = getParamIdx(varName);
    if (idx != NOT_FOUND) // if param exists
    {
        return CRS_SUCCESS;
    }

    // if can't save new param
    if (gNameValuesIdx == YARDEN_NAME_VALUES_SZ)
    {
        return CRS_FAILURE;
    }

    memcpy(gNameValues[gNameValuesIdx].name,varName, NAMEVALUE_NAME_SZ);

    char * varValue = myStrTok(NULL, sep);
    gNameValues[gNameValuesIdx].value = strtol(varValue, NULL, DECIMAL);
    gNameValuesIdx++;


    return CRS_SUCCESS;
}

static CRS_retVal_t wCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_W))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling w command");

    Yarden_wrContainer_t wrContainer = {0};
    initwrContainer(line, &wrContainer);
    //if its a global reg
//    if (isGlobal(line))
    if (isGlobal(&wrContainer))
    {
//        uint32_t addrVal = getAddress(line);

        uint32_t addrVal = 0;

        char val [20] = {0};
//        getVal(line, val);
        memcpy(val, wrContainer.val, sizeof(val));
        if (isStarValue(val))
        {
            handleStarGlobal(addrVal, val);

            return CRS_SUCCESS;
        }
        gGlobalReg[addrVal - GLOBAL_ADDR_START] = strtoul(val, NULL, 16);
        return CRS_SUCCESS;
    }

    // if address is not valid
    if(isInvalidAddr(&wrContainer))
    {
        return CRS_FAILURE;
    }

    uint32_t lutNumber;
    uint32_t lutReg;
    if (CRS_SUCCESS != getLutNumberFromWRContainer (&wrContainer, &lutNumber) ||
            CRS_SUCCESS != getLutRegFromWRContainer(&wrContainer, &lutReg))
    {
        return CRS_FAILURE;
    }

    char val [20] = {0};
//    getVal(line, val);
    memcpy(val, wrContainer.val, sizeof(val));
    if (isStarValue(val))
    {
        handleStarLut(lutNumber, lutReg, val);

        return CRS_SUCCESS;
    }

    gLineMatrix[lutNumber][lutReg] = strtoul(val, NULL, 16);

    return CRS_SUCCESS;
}

static CRS_retVal_t rCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_R))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling r command");


    return CRS_SUCCESS;
}

static CRS_retVal_t ewCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_EW))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling ew command");


    char lineToSend[100] = { 0 };
    char lineTemp[100] = { 0 };
    memcpy(lineTemp, line, 100);

    lineToSend[0] = 'w';
    lineToSend[1] = 'r';
    lineToSend[2] = ' ';
    const char s[2] = " ";
    char *token;
    token = myStrTok(lineTemp, s); //wr
    token = myStrTok(NULL, s); //addr
    strcat(lineToSend, token);
    token = strtok(NULL, s); //param or val
    int8_t idx = getParamIdx(token);
    if (NOT_FOUND == idx)
    {
        return CRS_FAILURE;
    }
    sprintf(lineToSend + strlen(lineToSend), " 0x%x",
                            gNameValues[idx].value);
    return CRS_SUCCESS;
}

static CRS_retVal_t erCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_ER))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling er command");

    return CRS_SUCCESS;
}

static CRS_retVal_t applyCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_APPLY))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling apply command");

    return CRS_SUCCESS;
}

static CRS_retVal_t labelCommandHandler (char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(line,CmdType_LABEL))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling label command");

    return CRS_SUCCESS;
}

static CRS_retVal_t gotoGivenLabel(char *label)
{
    strcat(label,":");
    char *label_ptr = strstr(gFileBuffer, label); // label_ptr is on start of label name
    if (NULL == label_ptr) // if label name is not found
    {
        return CRS_FAILURE;
    }

    while(*label_ptr != LINE_SEPARTOR) // iterate until \n
    {
        label_ptr++;
    }

    label_ptr ++; // go to the line where the label starts

    gIdxOfFileBuffer = label_ptr - gFileBuffer; // global index now on start of label

    return CRS_SUCCESS;
}


static int8_t getParamIdx(char *param)
{
    uint8_t i = 0;
    for(i = 0; i < gNameValuesIdx; i++)
    {
        if (0 == memcmp(gNameValues[i].name, param, strlen(param)))
        {
            return i;
        }
    }

    return NOT_FOUND;
}


//static bool isGlobal(char *line)
//{
//    uint32_t addrVal = 0;
//    addrVal = getAddress(line);
//    if (addrVal >= GLOBAL_ADDR_START && addrVal <= GLOBAL_ADDR_FINAL)
//    {
//        return true;
//    }
//
//    return false;
//}

static bool isGlobal(Yarden_wrContainer_t *wrContainer)
{
    uint32_t addrVal = 0;
    sscanf(wrContainer->addr, "%x", &addrVal);

    if (addrVal >= GLOBAL_ADDR_START && addrVal <= GLOBAL_ADDR_FINAL)
    {
        return true;
    }

    return false;
}

//w 0x1a10601c 0x0003
//returns 1c/4
//static uint32_t getAddress(char *line)
//{
//    const char s[2] = " ";
//    char tokenMode[5] = { 0 }; //w r ew er
//    char tokenAddr[15] = { 0 }; //0x1a10601c
//    char baseAddr[15] = { 0 };
//    char *ptr = line;
//    uint8_t i = 0;
//
//    while(*ptr != *s && i < sizeof(tokenMode)) // save mode (w)
//    {
//        tokenMode[i] = *ptr;
//        ptr++;
//        i++;
//    }
//    ptr++; // skip space
//
//    i = 0;
//    ptr += 2; // skip 0x
//    while(*ptr != *s && i < sizeof(tokenAddr)) // save address (1a10601c)
//    {
//        tokenAddr[i] = *ptr;
//        ptr++;
//        i++;
//    }
//
//
//    uint8_t size = i;
//    memcpy(baseAddr, tokenAddr + 6, size - 6); // skip first 6 digits in the hex to get the numerator (1c)
//
//    // get the final digits and divide them by 4 to get final address
//    uint32_t addrVal = 0;
//    sscanf(&baseAddr[0], "%x", &addrVal);
//    addrVal /= ADDRESS_CONVERT_RATIO;
//
//    return addrVal;
//}

//w 0x1a10601c 0x0003
//returns 0003
//static CRS_retVal_t getVal(char *line, char *ret)
//{
//    const char s[2] = " ";
//    char tokenMode[5] = { 0 }; //w r ew er
//    char tokenAddr[15] = { 0 }; //0x1a10601c
//    char baseAddr[15] = { 0 };
//
//    char val [25] = {0};
//    char *ptr = line;
//    uint8_t i = 0;
//    while(*ptr != *s && i < sizeof(tokenMode)) // save mode (w)
//    {
//        tokenMode[i] = *ptr;
//        ptr++;
//        i++;
//    }
//    ptr++; // skip space
//
//    i = 0;
//    ptr += 2; // skip 0x
//    while(*ptr != *s && i < sizeof(tokenAddr)) // save address (1a10601c)
//    {
//        tokenAddr[i] = *ptr;
//        ptr++;
//        i++;
//    }
//
//    ptr ++; // skip space
//
//    i = 0;
//    ptr+=2; // skip 0x or 16 or 32
//    while(*ptr != 0 && *ptr != *s && i < sizeof(val)) // save val (0003)
//    {
//        val[i] = *ptr;
//        ptr++;
//        i++;
//    }
//
//    uint8_t size = i;
//
//    memcpy(ret, val,size);
//
//    return CRS_SUCCESS;
//}

//static bool isInvalidAddr(char *line)
//{
//        uint32_t addrVal = getAddress(line);
//        if ((addrVal < 0x8 || addrVal > 0xf)
//                && (addrVal < GLOBAL_ADDR_START || addrVal > GLOBAL_ADDR_FINAL))
//        {
//            return true;
//        }
//
//        if(addrVal == 0x25)
//        {
//            return true;
//        }
//
//        return false;
//}

static bool isInvalidAddr(Yarden_wrContainer_t *wrContainer)
{

    uint32_t addrVal = 0;
    sscanf(wrContainer->addr, "%x", &addrVal);
    if ((addrVal < 0x8 || addrVal > 0xf)
            && (addrVal < GLOBAL_ADDR_START || addrVal > GLOBAL_ADDR_FINAL))
    {
        return true;
    }

    if(addrVal == 0x25)
    {
        return true;
    }

    return false;

}

static CRS_retVal_t getLutNumberFromWRContainer(Yarden_wrContainer_t *wrContainer, uint32_t *lutNumber)
{
    uint32_t addrVal = 0;
    sscanf(wrContainer->addr, "%x", &addrVal);

    if (addrVal < 0x8 || addrVal > 0x27)
    {
        return CRS_FAILURE;
    }

    if ((addrVal >= 0x26 && addrVal <= 0x27))
    {
        *lutNumber = 3;
    }
    else
    {
        *lutNumber = addrVal / 0x10;
    }

    return CRS_SUCCESS;
}
//static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber)
//{
//    uint32_t addrVal = getAddress(line);
//    if (addrVal < 0x8 || addrVal > 0x27)
//    {
//        return CRS_FAILURE;
//    }
//
//    if ((addrVal >= 0x26 && addrVal <= 0x27))
//    {
//        *lutNumber = 3;
//    }
//    else
//    {
//        *lutNumber = addrVal / 0x10;
//    }
//
//    return (CRS_SUCCESS);
//}



static CRS_retVal_t getLutRegFromWRContainer(Yarden_wrContainer_t *wrContainer, uint32_t *lutReg)
{
    uint32_t addrVal = 0;
    sscanf(wrContainer->addr, "%x", &addrVal);


    if (addrVal >= 0x26)
    {
        *lutReg = addrVal - 0x26;
    }
    else
    {
        *lutReg = addrVal % 8;
    }

    return CRS_SUCCESS;
}

//static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg)
//{
//    uint32_t addrVal = getAddress(line);
//    if (addrVal >= 0x26)
//    {
//        *lutReg = addrVal - 0x26;
//    }
//    else
//    {
//        *lutReg = addrVal % 8;
//    }
//
//    return CRS_SUCCESS;
//}

static CRS_retVal_t checkLineSyntax(char *line, enum command_type cmd)
{
    if (cmd == CmdType_COMMENT)
    {
        const char command [] = CMD_COMMENT;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }
    else if (cmd == CmdType_PARAMS)
    {
        const char command [] = CMD_PARAM;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_LABEL)
    {
        if(gFileBuffer[gIdxOfFileBuffer - 2] != ':') // next line - 1 (\n) -1 should be :
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_GOTO)
    {
        const char command [] = CMD_GOTO;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_W)
    {
        const char command [] = CMD_W;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_R)
    {
        const char command [] = CMD_R;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_EW)
    {
        const char command [] = CMD_EW;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }
    else if (cmd == CmdType_ER)
    {
        const char command [] = CMD_ER;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }
    else if (cmd == CmdType_IF)
    {
        const char sep [] = " ";
        char line_cpy [300] = {0};
        memcpy(line_cpy,line,sizeof(line_cpy));
        char *command = myStrTok(line_cpy, sep);// if_
        if (0 != memcmp(CMD_IF, command, strlen(command)))
        {
            return CRS_FAILURE;
        }
        command = myStrTok(NULL, sep);//var1_
        command = myStrTok(NULL, sep);//==_
        if (0 != memcmp("== ", command, strlen(command)))
        {
            return CRS_FAILURE;
        }
        command = myStrTok(NULL, sep);//var2_
        command = myStrTok(NULL, sep);//then_
        if (0 != memcmp("then ", command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_APPLY)
    {
        const char command [] = CMD_APPLY;
        if (0 != memcmp(line, command, strlen(line)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    return CRS_FAILURE;
}





static uint8_t isDelim(char c, const char *delim)
{
    while(*delim != '\0')
    {
        if(c == *delim)
        {
            return true;
        }

        delim++;
    }

    return false;
}

static char *myStrTok(char *srcString,const char *delim)
{
    static char *backup_string = NULL; // start of the next search
    if(!srcString)
    {
        srcString = backup_string;
    }

    if(!srcString)
    {
        // edge case where srcString is originally NULL
        return NULL;
    }

    // handle beginning of the string containing delims
    while(1)
    {
        if(isDelim(*srcString, delim))
        {
            srcString++;
            continue;
        }
        if(*srcString == '\0')
        {
            // we've reached the end of the string
            return NULL;
        }
        break;
    }

    char *ret = srcString;
    while(1)
    {
        if(*srcString == '\0')
        {
            /*end of the input string and
            next exec will return NULL*/
            backup_string = srcString;
            return ret;
        }
        if(isDelim(*srcString, delim))
        {
            *srcString = '\0';
            backup_string = srcString + 1;
            return ret;
        }

        srcString++;
    }
}



static CRS_retVal_t paramInit()
{
    gFileBuffer = NULL;
    gIdxOfFileBuffer = 0;
    memset(gNameValues,0,sizeof(gNameValues));
    gNameValuesIdx = 0;
    memset(gGlobalReg,0,sizeof(gGlobalReg));
    memset(gLineMatrix,0,sizeof(gLineMatrix));

    return CRS_SUCCESS;
}


static CRS_retVal_t saveNameVals(CRS_nameValue_t nameVals[YARDEN_NAME_VALUES_SZ])
{
    memcpy(gNameValues,nameVals,YARDEN_NAME_VALUES_SZ * sizeof(CRS_nameValue_t));
    CRS_nameValue_t *nameval_counter = &gNameValues[0];
    while(nameval_counter != NULL && (*nameval_counter).name[0] != 0)
    {
        gNameValuesIdx++;
        nameval_counter++;
    }

    return CRS_SUCCESS;
}


static bool isStarValue(char *val)
{
    const char starIndicator[] = "b'";
    if (0 == memcmp(val, starIndicator, strlen(starIndicator)))
    {
        return true;
    }

    return false;
}
static CRS_retVal_t handleStarLut(uint32_t lutNumber, uint32_t lutReg, char *val)
{
    uint32_t regVal = gLineMatrix[lutNumber][lutReg];
    getStarValue(val, &regVal);
    gLineMatrix[lutNumber][lutReg] = regVal;

    return CRS_SUCCESS;
}
static CRS_retVal_t handleStarGlobal(uint32_t addrVal, char *val)
{
    uint32_t regVal = gGlobalReg[addrVal - GLOBAL_ADDR_START];
    getStarValue(val, &regVal);
    gGlobalReg[addrVal - GLOBAL_ADDR_START] = regVal;

    return CRS_SUCCESS;
}


static CRS_retVal_t getStarValue(char *val, uint32_t *regVal)
{
    int i = 0;
    for (i = 0; i < 16; i++)
    {

        if (val[i] == '*')
        {
            continue;
        }
        else if (val[i] == '1')
        {
            *regVal |= (1 << (15 - i));
        }
        else if (val[i] == '0')
        {
            *regVal &= ~(1 << (15 - i));
        }
    }

    return CRS_SUCCESS;
}

static CRS_retVal_t initwrContainer(char *line, Yarden_wrContainer_t *ret)
{
    const char s[2] = " ";
    char *ptr = line;
    uint8_t i = 0;
    while(*ptr != *s && i < sizeof(ret->mode)) // save mode (w)
    {
        ret->mode[i] = *ptr;
        ptr++;
        i++;
    }
    ptr++; // skip space

    i = 0;
    ptr += 2; // skip 0x
    while(*ptr != *s && i < sizeof(ret->addr)) // save address (1a10601c)
    {
        ret->addr[i] = *ptr;
        ptr++;
        i++;
    }

    ptr ++; // skip space

    i = 0;
    ptr+=2; // skip 0x or 16 or 32
    while(*ptr != 0 && *ptr != *s && i < sizeof(ret->val)) // save val (0003)
    {
        ret->val[i] = *ptr;
        ptr++;
        i++;
    }

    return CRS_SUCCESS;
}
