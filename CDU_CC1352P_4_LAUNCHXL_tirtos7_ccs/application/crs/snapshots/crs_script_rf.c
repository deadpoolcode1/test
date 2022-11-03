/*
 * crs_script_rf.c
 *
 *  Created on: Oct 24, 2022
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <ctype.h> // isdigit
#include <ti/sysbios/knl/Task.h> // TODO erase at the end

#include "application/crs/snapshots/crs_script_rf.h"
#include "application/crs/crs_cli.h"
#include "application/crs/crs_nvs.h"
#include "application/crs/crs_tmp.h"
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
#define CMD_PRINT   "print "
#define CMD_DELAY   "delay "

#define LINE_SEPARTOR   '\n'
#define DECIMAL 10
#define HEXADECIMAL 16
#define NOT_FOUND   -1
#define ADDRESS_CONVERT_RATIO   4
#define LUT_REG_NUM 8
#define NUM_GLOBAL_REG 4
#define NUM_LUTS 4
#define LINE_LENGTH 50

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
    CmdType_EMPTY_LINE, // empty lines
    CmdType_PRINT,  // print
    CmdType_ASSIGNMENT, // "="
    CmdType_DELAY,  // "delay"
    CmdType_NUM_OF_COMMAND_TYPES
};



typedef struct wrStruct
{
    char mode[5]; //w r ew er
    char addr[15]; //1a10601c
    char val [20]; // 0003
    uint32_t finalAddr;
} ScriptRf_wrContainer_t;

typedef struct parsingStruct
{
    CRS_nameValue_t parameters[SCRIPT_RF_NAME_VALUES_SZ];
    uint8_t parametersIdx;
    uint16_t globalRegArray[NUM_GLOBAL_REG];
    uint16_t lineMatrix[NUM_LUTS][LUT_REG_NUM];
    char *fileBuffer;
    uint32_t idxOfFileBuffer;
    uint32_t chipNumber;
    uint32_t lineNumber;
    bool shouldUploadLine;
}ScriptRf_parsingContainer_t;

typedef CRS_retVal_t (*Line_Handler_fnx) (ScriptRf_parsingContainer_t *, char*);
static CRS_retVal_t getNextLine(ScriptRf_parsingContainer_t *parsingContainer, char *next_line);
static CRS_retVal_t handleLine(ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t checkLineSyntax(ScriptRf_parsingContainer_t *parsingContainer, char *line, enum command_type cmd);
static char *myStrTok(char *srcString,const char *delim);
static uint8_t isDelim(char c, const char *delim);

static CRS_retVal_t gotoGivenLabel(ScriptRf_parsingContainer_t *parsingContainer, char *label);

static int8_t getParamIdx(ScriptRf_parsingContainer_t *parsingContainer, char *param);

static bool isGlobal(ScriptRf_wrContainer_t *wrContainer);
static bool isInvalidAddr(ScriptRf_wrContainer_t *wrContainer);

static CRS_retVal_t getLutNumberFromWRContainer(ScriptRf_wrContainer_t *wrContainer, uint32_t *lutNumber);
static CRS_retVal_t getLutRegFromWRContainer(ScriptRf_wrContainer_t *wrContainer, uint32_t *lutReg);
static bool checkEquality(ScriptRf_parsingContainer_t *parsingContainer, char *p1, char *p2);
static bool isNumber(char* p);
static CRS_retVal_t readGlobalReg(uint32_t globalIdx, uint32_t *rsp);
static CRS_retVal_t readLutReg(uint32_t regIdx, uint32_t lutIdx, uint32_t *rsp);
static CRS_retVal_t readGlobalRegArray(ScriptRf_parsingContainer_t *parsingContainer);
static CRS_retVal_t readLineMatrix(ScriptRf_parsingContainer_t *parsingContainer);
static CRS_retVal_t printGlobalArrayAndLineMatrix(ScriptRf_parsingContainer_t *parsingContainer);


static CRS_retVal_t saveNameVals(ScriptRf_parsingContainer_t *parsingContainer, CRS_nameValue_t givenNameVals[SCRIPT_RF_NAME_VALUES_SZ]);
static bool isStarValue(char *val);
static CRS_retVal_t handleStarLut(ScriptRf_parsingContainer_t *parsingContainer, uint32_t lutNumber, uint32_t lutReg, char *val);
static CRS_retVal_t handleStarGlobal(ScriptRf_parsingContainer_t *parsingContainer, uint32_t addrVal, char *val);
static CRS_retVal_t getStarValue(char *val, uint32_t *regVal);
static CRS_retVal_t initwrContainer(char *line, ScriptRf_wrContainer_t *ret);

static CRS_retVal_t writeLutToFpga(ScriptRf_parsingContainer_t *parsingContainer,uint32_t lutNumber);
static CRS_retVal_t convertLutRegDataToStr(ScriptRf_parsingContainer_t *parsingContainer, uint32_t regIdx, uint32_t lutNumber,
                                           char *data);
static CRS_retVal_t convertLutRegToAddrStr(uint32_t regIdx, char *addr);

static CRS_retVal_t writeGlobalsToFpga(ScriptRf_parsingContainer_t *parsingContainer);
static CRS_retVal_t convertGlobalRegDataToStr(ScriptRf_parsingContainer_t *parsingContainer, uint32_t regIdx, char *data);
static CRS_retVal_t convertGlobalRegToAddrStr(uint32_t regIdx, char *addr);
static CRS_retVal_t delayRun(uint32_t time);
static CRS_retVal_t saveParamsToGainStateStruct(uint8_t *filename, CRS_nameValue_t nameVals[SCRIPT_RF_NAME_VALUES_SZ]);
static CRS_retVal_t writeLineAndGlobalsToFpga(ScriptRf_parsingContainer_t* parsingContainer);

// Command handlers
static CRS_retVal_t ifCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t gotoCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t commentCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t paramsCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t wCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t rCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t ewCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t erCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t applyCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t labelCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t emptyLineCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t delayCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t printCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);
static CRS_retVal_t assignmentCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line);







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
 labelCommandHandler,
 emptyLineCommandHandler,
 assignmentCommandHandler,
 printCommandHandler,
 delayCommandHandler
};




/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t scriptRf_init(void)
{
    return CRS_SUCCESS;
}

CRS_retVal_t scriptRf_runFile(uint8_t *filename, CRS_nameValue_t nameVals[SCRIPT_RF_NAME_VALUES_SZ], uint32_t chipNumber, uint32_t lineNumber)
{
    ScriptRf_parsingContainer_t parsingContainer = {0};
    parsingContainer.chipNumber = chipNumber;
    parsingContainer.lineNumber = lineNumber;
    parsingContainer.shouldUploadLine = false;

    saveParamsToGainStateStruct(filename, nameVals);

    char chipNumStr [LINE_LENGTH] = {0};
    char lineNumStr[LINE_LENGTH] = {0};

    sprintf(chipNumStr, "wr 0xff 0x%x\r", chipNumber);
    sprintf(lineNumStr, "wr 0xa 0x%x\r", lineNumber);

    printGlobalArrayAndLineMatrix(&parsingContainer);
    uint32_t rsp = 0;
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(chipNumStr,&rsp))
    {
        return CRS_FAILURE;
    }

    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lineNumStr,&rsp))
    {
        return CRS_FAILURE;
    }

    if (CRS_SUCCESS != readGlobalRegArray(&parsingContainer))
    {
        return CRS_FAILURE;
    }
    if (CRS_SUCCESS != readLineMatrix(&parsingContainer))
    {
        return CRS_FAILURE;
    }

    Task_sleep(4000);
    if (nameVals != NULL)
    {
        saveNameVals(&parsingContainer,nameVals);
    }

    printGlobalArrayAndLineMatrix(&parsingContainer);

    // read the file using nvs
    parsingContainer.fileBuffer = Nvs_readFileWithMalloc((char*)filename);
    if (NULL ==  parsingContainer.fileBuffer)
    {
        CLI_cliPrintf("File not found\r\n");
        return CRS_FAILURE;
    }

    uint32_t lenOfFile = strlen(parsingContainer.fileBuffer);

    char line[300] = {0};

    CRS_retVal_t retStatus = CRS_SUCCESS;
    do
    {
        // each line needs to get handled/translated and sent to FPGA
        memset(line, 0,sizeof(line));
        getNextLine(&parsingContainer,line);
        CLI_cliPrintf("\r\nline is %s",line);
        Task_sleep(5000);
        retStatus = handleLine(&parsingContainer, line);
        if (CRS_SUCCESS != retStatus)
        {
            CLI_cliPrintf("Syntax Error\r\n");
            return CRS_FAILURE;
        }
    }while (parsingContainer.idxOfFileBuffer < lenOfFile);

    writeLineAndGlobalsToFpga(&parsingContainer);
//
//    uint32_t i = 0;
//    for (i = 0; i < NUM_LUTS; i++)
//    {
//        writeLutToFpga(&parsingContainer, i);
//    }
//
//    writeGlobalsToFpga(&parsingContainer);
    printGlobalArrayAndLineMatrix(&parsingContainer);

    free (parsingContainer.fileBuffer);
    parsingContainer.fileBuffer = NULL;

    return retStatus;

}

/******************************************************************************
 Private Functions
 *****************************************************************************/
static CRS_retVal_t getNextLine(ScriptRf_parsingContainer_t *parsingContainer, char *next_line)
{
    char *ptr = &parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer];
    uint32_t len = 0;
    while(*ptr != LINE_SEPARTOR && *ptr != 0)
    {
        ptr++;
        len++;
    }
    memcpy(next_line, &parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer],len);
    parsingContainer->idxOfFileBuffer += len + 1;

    return CRS_SUCCESS;
}


static CRS_retVal_t handleLine(ScriptRf_parsingContainer_t *parsingContainer, char *line)
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

//if param == comparedVal then label
static CRS_retVal_t ifCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_IF))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling if command");
    char *comparedVal1 = NULL;
    char *comparedVal2 = NULL;
    char label[20] = { 0 };
    const char sep [] = " ";

//    int32_t comparedValInt = 0;
    char *ptr = line;

    ptr += strlen(CMD_IF); // skip "if "

    comparedVal1 = myStrTok(ptr, sep); // get param and skip "param "
    CLI_cliPrintf("parameter is %s",comparedVal1);

    myStrTok(NULL, sep); // skip ==
    comparedVal2 = myStrTok(NULL, sep); // get comparedVal and skip "comparedVal "

    if (false == checkEquality(parsingContainer, comparedVal1, comparedVal2))
    {
        return CRS_SUCCESS;
    }
//
//    int8_t idx = getParamIdx(param);
//    if (idx == NOT_FOUND)
//    {
//        return CRS_FAILURE;
//    }

//    comparedValInt = strtol(comparedVal, NULL, DECIMAL);
//    if(gNameValues[idx].value != comparedValInt) // if == is false
//    {
//        return CRS_SUCCESS;
//    }

    myStrTok(NULL, sep); // skip "then"
    ptr = myStrTok(NULL, sep);
    strcat(label, CMD_GOTO);
    strcat(label, ptr); // label says: "goto 'label'"
    gotoCommandHandler(parsingContainer, label);// run goto command on this

    return CRS_SUCCESS;
}

static CRS_retVal_t gotoCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_GOTO))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling goto command");
    const char sep [] = " ";
    char *token = myStrTok (line, sep); // goto
    token = myStrTok(NULL, sep);// label
    CLI_cliPrintf("the label is %s",token);
    gotoGivenLabel(parsingContainer, token);
    return CRS_SUCCESS;
}

static CRS_retVal_t commentCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_COMMENT))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling comment command");


    return CRS_SUCCESS;
}

static CRS_retVal_t paramsCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_PARAMS))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling params command");

    // move after //@@_
    line += strlen(CMD_PARAM);

    const char param_keyword [] = "param";
    uint8_t size = 5;
    if (0 != memcmp(line, param_keyword,size))
    {
        return CRS_SUCCESS;
    }

    // move after 'param' keyword
    line += size;
    line ++;
    char sep [] = ",";
    char *varName = myStrTok(line, sep);

    int8_t idx = getParamIdx(parsingContainer, varName);
    if (idx != NOT_FOUND) // if param exists
    {
        return CRS_SUCCESS;
    }

    // if can't save new param
    if (parsingContainer->parametersIdx == SCRIPT_RF_NAME_VALUES_SZ)
    {
        CLI_cliPrintf("\r\nno of space for new param");
        return CRS_FAILURE;
    }

    memcpy(parsingContainer->parameters[parsingContainer->parametersIdx].name,varName, NAMEVALUE_NAME_SZ);

    char * varValue = myStrTok(NULL, sep);
    parsingContainer->parameters[parsingContainer->parametersIdx].value = strtol(varValue, NULL, DECIMAL);
    parsingContainer->parametersIdx++;


    return CRS_SUCCESS;
}

static CRS_retVal_t wCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_W))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling w command");

    ScriptRf_wrContainer_t wrContainer = {0};
    initwrContainer(line, &wrContainer);
    //if its a global reg
//    if (isGlobal(line))
    if (isGlobal(&wrContainer))
    {
//        uint32_t addrVal = getAddress(line);

        uint32_t addrVal = 0;
        addrVal = wrContainer.finalAddr;
        char val [LINE_LENGTH] = {0};
//        getVal(line, val);
        memcpy(val, wrContainer.val, sizeof(val));
        if (isStarValue(val))
        {
            handleStarGlobal(parsingContainer, addrVal, val);

            return CRS_SUCCESS;
        }
        parsingContainer->globalRegArray[ addrVal - GLOBAL_ADDR_START ] = strtoul(val, NULL, HEXADECIMAL);
        CLI_cliPrintf("\r\n Inserting into globals, reg %x value %x",  addrVal - GLOBAL_ADDR_START , (uint32_t) parsingContainer->globalRegArray[ addrVal - GLOBAL_ADDR_START ]);

        return CRS_SUCCESS;
    }

    // if address is not valid
    if(isInvalidAddr(&wrContainer))
    {
        uint32_t addr = wrContainer.finalAddr;
        if (addr == 7) // only register address ignored
        {
            return CRS_SUCCESS;
        }
        CLI_cliPrintf("\r\naddress %s is not valid", wrContainer.addr);

        return CRS_FAILURE;
    }

    uint32_t lutNumber;
    uint32_t lutReg;
    if (CRS_SUCCESS != getLutNumberFromWRContainer (&wrContainer, &lutNumber) ||
            CRS_SUCCESS != getLutRegFromWRContainer(&wrContainer, &lutReg))
    {
        return CRS_FAILURE;
    }

    char val [LINE_LENGTH] = {0};
//    getVal(line, val);
    memcpy(val, wrContainer.val, sizeof(val));
    if (isStarValue(val))
    {
        handleStarLut(parsingContainer, lutNumber, lutReg, val);

        return CRS_SUCCESS;
    }

    parsingContainer->lineMatrix[lutNumber][lutReg] = strtoul(val, NULL, HEXADECIMAL);
    CLI_cliPrintf("\r\n Inserting into lut %d reg %d value %x", lutNumber, lutReg, (uint32_t)parsingContainer->lineMatrix[lutNumber][lutReg]);

    return CRS_SUCCESS;
}

static CRS_retVal_t rCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_R))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling r command");


    return CRS_SUCCESS;
}

static CRS_retVal_t ewCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_EW))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling ew command");


    char lineToSend[LINE_LENGTH] = { 0 };
    char lineTemp[LINE_LENGTH] = { 0 };
    memcpy(lineTemp, line, LINE_LENGTH);

    lineToSend[0] = 'w';
    lineToSend[1] = 'r';
    lineToSend[2] = ' ';
    const char s[2] = " ";
    char *token;
    token = myStrTok(lineTemp, s); //wr
    token = myStrTok(NULL, s); //addr
    strcat(lineToSend, token);
    token = strtok(NULL, s); //param or val
    int8_t idx = getParamIdx(parsingContainer, token);
    if (NOT_FOUND == idx)
    {
        return CRS_FAILURE;
    }
    sprintf(lineToSend + strlen(lineToSend), " 0x%x",
            parsingContainer->parameters[idx].value);

    return CRS_SUCCESS;
}

static CRS_retVal_t erCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_ER))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling er command");

    return CRS_SUCCESS;
}

static CRS_retVal_t applyCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_APPLY))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling apply command");

    parsingContainer->shouldUploadLine = true;

//    writeLineAndGlobalsToFpga(parsingContainer);

    return CRS_SUCCESS;
}

static CRS_retVal_t labelCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line,CmdType_LABEL))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling label command");

    return CRS_SUCCESS;
}


static CRS_retVal_t emptyLineCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_EMPTY_LINE))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling empty line command");

    return CRS_SUCCESS;
}


static CRS_retVal_t delayCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_DELAY))
    {
        return CRS_FAILURE;
    }

    CLI_cliPrintf("handling delay command");

    char *ptr = line;
    ptr += strlen(CMD_DELAY);
    uint32_t time = strtoul(ptr, NULL, DECIMAL);
    delayRun(time);

    return CRS_SUCCESS;
}


static CRS_retVal_t printCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_PRINT))
    {
        return CRS_FAILURE;
    }

    CLI_cliPrintf("handling print command");

    char *ptr = line;
    ptr += strlen(CMD_PRINT); // skip 'print '

    if (*ptr == '"')
    {
        ptr ++;
        CLI_cliPrintf("\r\n");
        while(*ptr != '"')
        {
            CLI_cliPrintf("%c", *ptr);
            ptr++;
        }
        ptr += 2; // skip '" '
    }

    char param[NAMEVALUE_NAME_SZ] = {0};
    uint8_t i = 0;
    while (ptr != NULL && *ptr != 0 && *ptr != LINE_SEPARTOR)
    {
        param[i] = *ptr;
        ptr++;
        i++;
    }

    int8_t idx = getParamIdx(parsingContainer, param);
    if (idx == NOT_FOUND)
    {
        CLI_cliPrintf("\r\nparam %s not found");
        return CRS_FAILURE;
    }
    CLI_cliPrintf("\r\n%d", parsingContainer->parameters[idx].value);

    return CRS_SUCCESS;
}

static CRS_retVal_t assignmentCommandHandler (ScriptRf_parsingContainer_t *parsingContainer, char *line)
{
    if (CRS_SUCCESS != checkLineSyntax(parsingContainer, line, CmdType_ASSIGNMENT))
    {
        return CRS_FAILURE;
    }

    CLI_cliPrintf("handling assignment command");

    char paramOne[NAMEVALUE_NAME_SZ] = {0};
    char *ptr = line;
    uint8_t i = 0;

    while(*ptr != ' ')
    {
        paramOne[i] = *ptr;
        ptr++;
        i++;
    }

    int8_t idxParamOne = getParamIdx(parsingContainer, paramOne);
    if (idxParamOne == NOT_FOUND)
    {
        CLI_cliPrintf("\r\nparam %s not found", paramOne);
        return CRS_FAILURE;
    }

    ptr += strlen(" = ");
    char paramTwo[NAMEVALUE_NAME_SZ] = {0};
    i = 0;
    while(*ptr && *ptr != LINE_SEPARTOR)
    {
        paramTwo[i] = *ptr;
        ptr++;
        i++;
    }

    if(isNumber(paramTwo))
    {
        int32_t paramTwoNum = strtol(paramTwo, NULL, DECIMAL);
        parsingContainer->parameters[idxParamOne].value = paramTwoNum;

        return CRS_SUCCESS;
    }

    int8_t idxParamTwo = getParamIdx(parsingContainer, paramTwo);
    if (idxParamTwo == NOT_FOUND)
    {
        CLI_cliPrintf("\r\nparam %s not found", paramTwo);
        return CRS_FAILURE;
    }

    parsingContainer->parameters[idxParamOne].value = parsingContainer->parameters[idxParamTwo].value;

    return CRS_SUCCESS;
}




static CRS_retVal_t gotoGivenLabel(ScriptRf_parsingContainer_t *parsingContainer, char *label)
{
    strcat(label,":");
    char *label_ptr = strstr(parsingContainer->fileBuffer, label); // label_ptr is on start of label name
    if (NULL == label_ptr) // if label name is not found
    {
        return CRS_FAILURE;
    }

    while(*label_ptr != LINE_SEPARTOR) // iterate until \n
    {
        label_ptr++;
    }

    label_ptr ++; // go to the line where the label starts

    parsingContainer->idxOfFileBuffer = label_ptr - parsingContainer->fileBuffer; // global index now on start of label

    return CRS_SUCCESS;
}


static int8_t getParamIdx(ScriptRf_parsingContainer_t *parsingContainer, char *param)
{
    uint8_t i = 0;
    for(i = 0; i < parsingContainer->parametersIdx; i++)
    {
        if (0 == memcmp(parsingContainer->parameters[i].name, param, strlen(param)))
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

static bool isGlobal(ScriptRf_wrContainer_t *wrContainer)
{
//    uint32_t addrVal = 0;
//    sscanf(wrContainer->addr, "%x", &addrVal);
    uint32_t addrVal = wrContainer->finalAddr;

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

static bool isInvalidAddr(ScriptRf_wrContainer_t *wrContainer)
{

    //    uint32_t addrVal = 0;
    //    sscanf(wrContainer->addr, "%x", &addrVal);
        uint32_t addrVal = wrContainer->finalAddr;

    if ((addrVal < 0x8 || addrVal > 0x27)
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

static CRS_retVal_t getLutNumberFromWRContainer(ScriptRf_wrContainer_t *wrContainer, uint32_t *lutNumber)
{
    //    uint32_t addrVal = 0;
    //    sscanf(wrContainer->addr, "%x", &addrVal);
        uint32_t addrVal = wrContainer->finalAddr;

        if ((addrVal >= 0x8 && addrVal <= 0xf))
        {
            *lutNumber = 0;
            return (CRS_SUCCESS);

        }

        if ((addrVal >= 0x10 && addrVal <= 0x17))
        {
            *lutNumber = 1;
            return (CRS_SUCCESS);
        }

        if ((addrVal >= 0x18 && addrVal <= 0x1f))
        {
            *lutNumber = 2;
            return (CRS_SUCCESS);
        }

        if ((addrVal >= 0x26 && addrVal <= 0x27))
        {
            *lutNumber = 3;
            return (CRS_SUCCESS);
        }

        return (CRS_FAILURE);

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



static CRS_retVal_t getLutRegFromWRContainer(ScriptRf_wrContainer_t *wrContainer, uint32_t *lutReg)
{
    //    uint32_t addrVal = 0;
    //    sscanf(wrContainer->addr, "%x", &addrVal);
        uint32_t addrVal = wrContainer->finalAddr;


    if (addrVal >= 0x26)
    {
        *lutReg = addrVal - 0x26;
    }
    else
    {
        *lutReg = addrVal % 8; //each lut length is 8
    }

    return CRS_SUCCESS;
}

static CRS_retVal_t checkLineSyntax(ScriptRf_parsingContainer_t *parsingContainer, char *line, enum command_type cmd)
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
        if(parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer - 2] != ':') // next line - 1 (\n) -1 should be :
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
        if (0 != memcmp(line, command, strlen(command)))
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

    else if (cmd == CmdType_ASSIGNMENT)
    {
        if (line[0] == '=' || // first char in line is '='
                parsingContainer->fileBuffer[parsingContainer->idxOfFileBuffer - 2] == '=' || // last char in line is '='
                strchr(line, '=') == NULL // no '=' in line
                )
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_DELAY)
    {
        const char command [] = CMD_DELAY;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        return CRS_SUCCESS;
    }

    else if (cmd == CmdType_PRINT)
    {
        const char command [] = CMD_PRINT;
        if (0 != memcmp(line, command, strlen(command)))
        {
            return CRS_FAILURE;
        }

        char *ptr = line;
        ptr += strlen(CMD_PRINT);

        if(*ptr == '"') // if starting brackets appear make sure of closing brackets
        {
            ptr ++;
            while (*ptr != 0 && *ptr != LINE_SEPARTOR && *ptr != '"')
            {
                ptr++;
            }

            if (*ptr != '"')
            {
                return CRS_FAILURE;
            }
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



//static CRS_retVal_t paramInit()
//{
//    gFileBuffer = NULL;
//    gIdxOfFileBuffer = 0;
//    memset(gNameValues,0,sizeof(gNameValues));
//    gNameValuesIdx = 0;
//    memset(gGlobalReg,0,sizeof(gGlobalReg));
//    memset(gLineMatrix,0,sizeof(gLineMatrix));
//
//    return CRS_SUCCESS;
//}


static CRS_retVal_t saveNameVals(ScriptRf_parsingContainer_t *parsingContainer, CRS_nameValue_t givenNameVals[SCRIPT_RF_NAME_VALUES_SZ])
{
    memcpy(parsingContainer->parameters,givenNameVals,SCRIPT_RF_NAME_VALUES_SZ * sizeof(CRS_nameValue_t));
    CRS_nameValue_t *nameval_iterator = parsingContainer->parameters;
    while(nameval_iterator != NULL && (*nameval_iterator).name[0] != 0)
    {
        parsingContainer->parametersIdx++;
        nameval_iterator++;
//        CLI_cliPrintf("param is %s with value %x\r\n", nameVals[nameValuesIdx].name, nameVals[nameValuesIdx].value);
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
static CRS_retVal_t handleStarLut(ScriptRf_parsingContainer_t *parsingContainer, uint32_t lutNumber, uint32_t lutReg, char *val)
{
    uint32_t regVal =parsingContainer->lineMatrix[lutNumber][lutReg];
    val += 2; // skip b'
    getStarValue(val, &regVal);
    parsingContainer->lineMatrix[lutNumber][lutReg] = regVal;
    CLI_cliPrintf("\r\n Inserting into lut %d reg %d value %x", lutNumber, lutReg, (uint32_t)parsingContainer->lineMatrix[lutNumber][lutReg]);

    return CRS_SUCCESS;
}
static CRS_retVal_t handleStarGlobal(ScriptRf_parsingContainer_t *parsingContainer, uint32_t addrVal, char *val)
{
    uint32_t regVal = parsingContainer->globalRegArray[ addrVal - GLOBAL_ADDR_START ];
    val += 2; // skip b'
    getStarValue(val, &regVal);
    parsingContainer->globalRegArray[ addrVal - GLOBAL_ADDR_START ] = regVal;

    return CRS_SUCCESS;
}


static CRS_retVal_t getStarValue(char *val, uint32_t *regVal)
{
    CLI_cliPrintf("the line is %s", val);
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

static CRS_retVal_t initwrContainer(char *line, ScriptRf_wrContainer_t *ret)
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

//    uint32_t fullAddr = 0;
//    sscanf(ret->addr, "%x",&fullAddr);
//
////    uint32_t partAddr = 0;
////    char partAddrStr [LINE_LENGTH] = {0};
////    memcpy(partAddrStr, ret->addr, 5); // copy first 5 digits of addr
////    sscanf(partAddrStr, "%x",&partAddr);
////
////    uint32_t addr = fullAddr - partAddr;
//    char *finalAddrStr = ret->addr + 5;
//    uint32_t addr = 0;
//    sscanf(finalAddrStr, "%x",&addr);

    uint32_t fullAddr = strtoul(ret->addr,NULL,16);
    char partAddrStr [LINE_LENGTH] = {0};
    memset(partAddrStr, '0', 8);

    memcpy(partAddrStr, ret->addr, 5); // copy first 5 digits of addr
    uint32_t partAddr =strtoul(partAddrStr, NULL, 16);
    uint64_t addr = (fullAddr - partAddr)/4;

   // addr /= ADDRESS_CONVERT_RATIO;
    ret->finalAddr = addr;

    if(0 == memcmp(ret->mode,"r", strlen("r")) || 0 == memcmp(ret->mode, "er", strlen("er")))
    {
        return CRS_SUCCESS;
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

    CLI_cliPrintf("mode is %s\r\n addr is %s\r\n val is %s final address is %d", ret->mode, ret->addr, ret->val, ret->finalAddr);
    Task_sleep(1000);
    return CRS_SUCCESS;
}

static bool checkEquality(ScriptRf_parsingContainer_t *parsingContainer, char *p1, char *p2)
{
    //check for both if they are numbers
    bool isNumberP1 = isNumber(p1);
    bool isNumberP2 = isNumber(p2);
    if (isNumberP1 && isNumberP2)
    {
        //both are numbers
        uint32_t numP1 = strtoul(p1, NULL, DECIMAL);
        uint32_t numP2 = strtoul(p2, NULL, DECIMAL);

        return numP1 == numP2;
    }

    else if (isNumberP1)
    {
        // p2 must be a parameter
        int8_t idx = getParamIdx(parsingContainer, p2);
        if(NOT_FOUND == idx)
        {
            return false;
        }

        uint32_t numP1 = strtoul(p1, NULL, DECIMAL);


        return parsingContainer->parameters[idx].value == numP1;
    }

    else if (isNumberP2)
    {
        //p1 must be parameter
        int8_t idx = getParamIdx(parsingContainer, p1);
        if(NOT_FOUND == idx)
        {
            return false;
        }

        uint32_t numP2 = strtoul(p2, NULL, DECIMAL);


        return parsingContainer->parameters[idx].value == numP2;
    }

    else
    {
        // both must be parameters
        int8_t idxP1 = getParamIdx(parsingContainer, p1);
        if(NOT_FOUND == idxP1)
        {
            return false;
        }

        int8_t idxP2 = getParamIdx(parsingContainer, p2);
        if(NOT_FOUND == idxP2)
        {
            return false;
        }

        return parsingContainer->parameters[idxP1].value == parsingContainer->parameters[idxP2].value;
    }

}


static bool isNumber(char* p)
{
    char *ptr = p;
    if (ptr == NULL)
    {
        return false;
    }

    if (*ptr == '-' || *ptr == '+')
    {
        ptr++;
    }

    while(ptr != NULL && *ptr != 0 && *ptr != LINE_SEPARTOR)
    {
        if(!isdigit(*ptr))
        {
            return false;
        }
        ptr++;
    }

    return true;
}



static CRS_retVal_t readLutReg(uint32_t regIdx, uint32_t lutIdx, uint32_t *rsp)
{
    char lines[9][CRS_NVS_LINE_BYTES] = { 0 };

    sprintf(lines[0], "wr 0x50 0x3900%x%x\r", regIdx, lutIdx);
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[0], rsp))
    {
        return CRS_FAILURE;
    }

    memcpy(lines[1], "wr 0x50 0x000001\r", strlen("wr 0x50 0x000001\r"));
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[1], rsp))
    {
        return CRS_FAILURE;
    }

    memcpy(lines[2], "wr 0x50 0x000000\r", strlen("wr 0x50 0x000000\r"));
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[2], rsp))
    {
        return CRS_FAILURE;
    }
    memcpy(lines[3], "wr 0x51 0x510000\r", strlen("wr 0x51 0x510000\r"));
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[3], rsp))
    {
        return CRS_FAILURE;
    }

    memcpy(lines[4], "rd 0x51\r", strlen("rd 0x51\r"));
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[4], rsp))
    {
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}

static CRS_retVal_t readGlobalReg(uint32_t globalIdx, uint32_t *rsp)
{
    char lines[9][CRS_NVS_LINE_BYTES] = { 0 };
    sprintf(lines[0], "wr 0x51 0x%x0000\r", globalIdx + GLOBAL_ADDR_START);
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[0], rsp))
    {
        return CRS_FAILURE;
    }

    memcpy(lines[1], "rd 0x51\r", strlen("rd 0x51\r"));
    if (CRS_SUCCESS != Fpga_tmpWriteMultiLine(lines[1], rsp))
    {
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}


static CRS_retVal_t readGlobalRegArray(ScriptRf_parsingContainer_t *parsingContainer)
{
    uint32_t regIdx = 0;

    for(regIdx = 0; regIdx < NUM_GLOBAL_REG; regIdx++)
    {
        uint32_t rsp = 0;
        if (CRS_SUCCESS != readGlobalReg(regIdx, &rsp))
        {
            return CRS_FAILURE;
        }
        parsingContainer->globalRegArray[regIdx] = rsp;
    }

    return CRS_SUCCESS;
}


static CRS_retVal_t readLineMatrix(ScriptRf_parsingContainer_t *parsingContainer)
{
    uint32_t lutIdx = 0;
    uint32_t regIdx = 0;

    for (lutIdx = 0; lutIdx < NUM_LUTS; lutIdx ++)
    {
        for (regIdx = 0; regIdx < LUT_REG_NUM; regIdx++)
        {
            uint32_t rsp = 0;
            if (CRS_SUCCESS != readLutReg(regIdx, lutIdx, &rsp))
            {
                return CRS_FAILURE;
            }

            parsingContainer->lineMatrix[lutIdx][regIdx] = rsp;
            Task_sleep(1000);
        }
    }

    return CRS_SUCCESS;
}






static CRS_retVal_t printGlobalArrayAndLineMatrix(ScriptRf_parsingContainer_t *parsingContainer)
{
    uint16_t i = 0;
    CLI_cliPrintf("\r\nPrinting globals\r\n");
    for (i = 0; i < NUM_GLOBAL_REG; i++)
    {
        CLI_cliPrintf("reg %x: %x, ",(uint32_t)i,(uint32_t)parsingContainer->globalRegArray[i]);
    }
    CLI_cliPrintf("\r\nPrinting Line Matrix\r\n");

    uint16_t j = 0;
    for (i = 0; i < NUM_LUTS; i++)
    {
        CLI_cliPrintf("\r\nlut %d:\r\n",i);
        for (j = 0; j < LUT_REG_NUM; j++)
        {
            CLI_cliPrintf("reg %x: %x, ",(uint32_t)j,(uint32_t)parsingContainer->lineMatrix[i][j]);
        }
    }

    return CRS_SUCCESS;
}





static CRS_retVal_t writeLutToFpga(ScriptRf_parsingContainer_t *parsingContainer,uint32_t lutNumber)
{
    char *startSeq = "wr 0x50 0x430000\nwr 0x50 0x000001\nwr 0x50 0x000000";
    char endSeq[100] = { 0 };
    sprintf(endSeq, "\nwr 0x50 0x430%x0%x\nwr 0x50 0x000001\nwr 0x50 0x000000",
            parsingContainer->lineNumber, (1 << lutNumber));
    char lines[600] = { 0 };
    memcpy(lines, startSeq, strlen(startSeq));
    int x = 0;
    for (x = 0; x < 8; x++)
    {
        char addr[11] = { 0 };
        char val[11] = { 0 };

        convertLutRegToAddrStr(x, addr);
        convertLutRegDataToStr(parsingContainer, x, lutNumber, val);
        sprintf(&lines[strlen(lines)], "\nwr 0x50 0x%s%s", addr, val);
    }
    sprintf(&lines[strlen(lines)], "%s", endSeq);

    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(lines, &rsp);

    return (CRS_SUCCESS);

}




static CRS_retVal_t convertLutRegDataToStr(ScriptRf_parsingContainer_t *parsingContainer, uint32_t regIdx, uint32_t lutNumber,
                                           char *data)
{
    char valStr[10] = { 0 };
    memset(valStr, '0', 4);
    uint16_t val = parsingContainer->lineMatrix[lutNumber][regIdx];
    char tmp[11] = { 0 };
    sprintf(tmp, "%x", val);
    sprintf(&valStr[4 - strlen(tmp)], "%s", tmp);
    memcpy(data, valStr, 4);
    return (CRS_SUCCESS);
}

static CRS_retVal_t convertLutRegToAddrStr(uint32_t regIdx, char *addr)
{
    switch (regIdx)
    {
    case 0:
        memcpy(addr, "44", 2);
        break;
    case 1:
        memcpy(addr, "45", 2);
        break;
    case 2:
        memcpy(addr, "46", 2);
        break;
    case 3:
        memcpy(addr, "47", 2);
        break;
    case 4:
        memcpy(addr, "48", 2);
        break;
    case 5:
        memcpy(addr, "49", 2);
        break;
    case 6:
        memcpy(addr, "4a", 2);
        break;
    case 7:
        memcpy(addr, "4b", 2);
        break;
    }
    return (CRS_SUCCESS);
}


static CRS_retVal_t writeGlobalsToFpga(ScriptRf_parsingContainer_t *parsingContainer)
{

    char lines[LINE_LENGTH * 12] = { 0 };
    int x = 0;
    for (x = 0; x < NUM_GLOBAL_REG; x++)
    {
        char addr[LINE_LENGTH] = { 0 };
        char val[LINE_LENGTH] = { 0 };

        convertGlobalRegToAddrStr(x, addr);
        convertGlobalRegDataToStr(parsingContainer, x, val);
        sprintf(&lines[strlen(lines)], "wr 0x50 0x%s%s\n", addr, val);
    }
    lines[strlen(lines) - 1] = 0;

    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(lines, &rsp);

    return CRS_SUCCESS;
}


static CRS_retVal_t convertGlobalRegDataToStr(ScriptRf_parsingContainer_t *parsingContainer, uint32_t regIdx, char *data)
{
    char valStr[10] = { 0 };
    memset(valStr, '0', 4);
    uint16_t val = parsingContainer->globalRegArray[regIdx];
    char tmp[11] = { 0 };
    sprintf(tmp, "%x", val);
    sprintf(&valStr[4 - strlen(tmp)], "%s", tmp);
    memcpy(data, valStr, 4);
    return (CRS_SUCCESS);
}

static CRS_retVal_t convertGlobalRegToAddrStr(uint32_t regIdx, char *addr)
{
    switch (regIdx)
    {
    case 0:
        memcpy(addr, "3f", 2);
        break;
    case 1:
        memcpy(addr, "40", 2);
        break;
    case 2:
        memcpy(addr, "41", 2);
        break;
    case 3:
        memcpy(addr, "42", 2);
        break;

    }
    return (CRS_SUCCESS);
}

static CRS_retVal_t delayRun(uint32_t time)
{
    Task_sleep(time);
    return CRS_SUCCESS;
}


static CRS_retVal_t saveParamsToGainStateStruct(uint8_t *filename, CRS_nameValue_t nameVals[SCRIPT_RF_NAME_VALUES_SZ])
{
    if (memcmp(filename, "DC_RF_HIGH_FREQ_HB_RX",
                     sizeof("DC_RF_HIGH_FREQ_HB_RX")-1) == 0 && nameVals != NULL)
    {
      CRS_cbGainStates.dc_rf_high_freq_hb_rx = nameVals[0].value;

    }
    else if (memcmp(filename, "DC_IF_LOW_FREQ_TX",
                  sizeof("DC_IF_LOW_FREQ_TX") -1) == 0 && nameVals != NULL)
    {
      CRS_cbGainStates.dc_if_low_freq_tx = nameVals[0].value;

    }
    else if (memcmp(filename, "UC_RF_HIGH_FREQ_HB_TX",
                  sizeof("UC_RF_HIGH_FREQ_HB_TX")-1) == 0 && nameVals != NULL)
    {
      CRS_cbGainStates.uc_rf_high_freq_hb_tx = nameVals[0].value;

    }
    else if (memcmp(filename, "UC_IF_LOW_FREQ_RX",
                  sizeof("UC_IF_LOW_FREQ_RX")-1) == 0 && nameVals != NULL)
    {
      CRS_cbGainStates.uc_if_low_freq_rx = nameVals[0].value;

    }

    return CRS_SUCCESS;
}
static CRS_retVal_t writeLineAndGlobalsToFpga(ScriptRf_parsingContainer_t* parsingContainer)
{
    writeGlobalsToFpga(parsingContainer);

    if (parsingContainer->shouldUploadLine == false)
    {
        return CRS_SUCCESS;
    }

    uint32_t i = 0;
    for (i = 0; i < NUM_LUTS; i++)
    {
        writeLutToFpga(parsingContainer, i);
    }


    return CRS_SUCCESS;
}
