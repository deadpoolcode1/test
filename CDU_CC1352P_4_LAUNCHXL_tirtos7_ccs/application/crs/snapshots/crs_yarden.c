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
} Label_index_t;

typedef CRS_retVal_t (*Line_Handler_fnx) (char*);
static char *getNextLine(void);
static CRS_retVal_t handleLine(char *line);
static void free_gFileBuffer(void);
static CRS_retVal_t CheckLineSyntax(char *line, enum command_type cmd);
static char *MyStrTok(char *srcString,const char *delim);
static uint8_t is_delim(char c, const char *delim);

static CRS_retVal_t GotoGivenLabel(char *label);

/* this function returns the index of given param in
 * global array,
 * if not found in array return NOT_FOUND (-1)
 */
static int8_t getParamIdx(char *param);
static bool isGlobal(char *line);
static uint32_t getAddress(char *line);
static CRS_retVal_t getVal(char *line, char *ret);
static bool isInvalidAddr(char *line);
static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber);
static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg);


// Command handlers
static CRS_retVal_t IfCommandHandler (char *line);
static CRS_retVal_t GotoCommandHandler (char *line);
static CRS_retVal_t CommentCommandHandler (char *line);
static CRS_retVal_t ParamsCommandHandler (char *line);
static CRS_retVal_t WCommandHandler (char *line);
static CRS_retVal_t RCommandHandler (char *line);
static CRS_retVal_t EWCommandHandler (char *line);
static CRS_retVal_t ERCommandHandler (char *line);
static CRS_retVal_t ApplyCommandHandler (char *line);
static CRS_retVal_t LabelCommandHandler (char *line);





/******************************************************************************
 Local variables
 *****************************************************************************/
static Line_Handler_fnx gFunctionsTable[CmdType_NUM_OF_COMMAND_TYPES] =
{
 IfCommandHandler,
 GotoCommandHandler,
 CommentCommandHandler,
 ParamsCommandHandler,
 WCommandHandler,
 RCommandHandler,
 EWCommandHandler,
 ERCommandHandler,
 ApplyCommandHandler,
 LabelCommandHandler
};

static char *gFileBuffer = NULL;
static uint8_t gIdxOfFileBuffer = 0;
static CRS_nameValue_t gNameValues[YARDEN_NAME_VALUES_SZ];
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
    // read the file using nvs
    gFileBuffer = Nvs_readFileWithMalloc((char*)filename);
    if (NULL == gFileBuffer)
    {
        return CRS_FAILURE;
    }

    char *line = getNextLine();
    CRS_retVal_t retStatus = CRS_SUCCESS;

    while (NULL != line)
    {
        // each line needs to get handled/translated and sent to FPGA
        retStatus = handleLine(line);
        if (CRS_SUCCESS != retStatus)
        {
//            do nothing
        }

        line = getNextLine();
    }

    free_gFileBuffer();
    return retStatus;

}

/******************************************************************************
 Private Functions
 *****************************************************************************/
static char *getNextLine(void)
{
    const char s[2] = "\n";
    return MyStrTok(&(gFileBuffer[gIdxOfFileBuffer]), s);
}

static void free_gFileBuffer(void)
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
static CRS_retVal_t IfCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_IF))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling if command");
    char *param = NULL;
    char *comparedVal = NULL;
    char label[20] = { 0 };
    const char sep [] = " ";
    strcat(label, CMD_GOTO);
    int32_t comparedValInt = 0;
    char *ptr = line;

    ptr += strlen(CMD_IF); // skip "if "

    param = MyStrTok(ptr, sep); // get param and skip "param "
    ptr += strlen("== "); // skip ==
    comparedVal = MyStrTok(NULL, sep); // get comparedVal and skip "comparedVal "

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

    ptr += strlen("then "); // skip "then "
    strcat(label, ptr); // label says: "goto 'label'"
    GotoCommandHandler(label);// run goto command on this

    return CRS_SUCCESS;
}

static CRS_retVal_t GotoCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_GOTO))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling goto command");
    const char sep [] = " ";
    char *token = MyStrTok (line, sep); // goto
    token = MyStrTok(NULL, sep);// label

    GotoGivenLabel(token);
    return CRS_SUCCESS;
}

static CRS_retVal_t CommentCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_COMMENT))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling comment command");


    return CRS_SUCCESS;
}

static CRS_retVal_t ParamsCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_PARAMS))
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
    char *varName = MyStrTok(line, sep);

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

    char * varValue = MyStrTok(NULL, sep);
    gNameValues[gNameValuesIdx].value = strtol(varValue, NULL, DECIMAL);
    gNameValuesIdx++;


    return CRS_SUCCESS;
}

static CRS_retVal_t WCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_W))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling w command");

    //if its a global reg
    if (isGlobal(line))
    {
        uint32_t addrVal = getAddress(line);
        char val [20] = {0};
        getVal(line, val);
        gGlobalReg[addrVal - GLOBAL_ADDR_START] = strtoul(val, NULL, 16);
        return CRS_SUCCESS;
    }

    // if address is not valid
    if(isInvalidAddr(line))
    {
        return CRS_FAILURE;
    }

    uint32_t lutNumber;
    uint32_t lutReg;
    if (CRS_SUCCESS != getLutNumberFromLine(line, &lutNumber) ||
            CRS_SUCCESS != getLutRegFromLine(line, &lutReg))
    {
        return CRS_FAILURE;
    }

    char val [20] = {0};
    getVal(line, val);

    gLineMatrix[lutNumber][lutReg] = strtoul(val, NULL, 16);

    return CRS_SUCCESS;
}

static CRS_retVal_t RCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_R))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling r command");


    return CRS_SUCCESS;
}

static CRS_retVal_t EWCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_EW))
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
    token = MyStrTok(lineTemp, s); //wr
    token = MyStrTok(NULL, s); //addr
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

static CRS_retVal_t ERCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_ER))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling er command");

    return CRS_SUCCESS;
}

static CRS_retVal_t ApplyCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_APPLY))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling apply command");

    return CRS_SUCCESS;
}

static CRS_retVal_t LabelCommandHandler (char *line)
{
    if (CRS_SUCCESS != CheckLineSyntax(line,CmdType_LABEL))
    {
        return CRS_FAILURE;
    }
    CLI_cliPrintf("handling label command");

    return CRS_SUCCESS;
}

static CRS_retVal_t GotoGivenLabel(char *label)
{
    char *label_ptr = strstr(gFileBuffer, label); // label_ptr is on start of label name
    if (NULL == label_ptr) // if label name is not found
    {
        return CRS_FAILURE;
    }
    label_ptr += strlen(label_ptr) + 1; // label_ptr now on start of the label
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


static bool isGlobal(char *line)
{
    uint32_t addrVal = 0;
    addrVal = getAddress(line);
    if (addrVal >= GLOBAL_ADDR_START && addrVal <= GLOBAL_ADDR_FINAL)
    {
        return true;
    }

    return false;
}

//w 0x1a10601c 0x0003
//returns 1c/4
static uint32_t getAddress(char *line)
{
    const char s[2] = " ";
    char *token;
    char tokenMode[5] = { 0 }; //w r ew er
    char tokenAddr[15] = { 0 }; //0x1a10601c
    char baseAddr[15] = { 0 };
    memset(baseAddr, '0', 8);

    char *ptr = line;
    token = MyStrTok(ptr, s); // w_
    memcpy(tokenMode, token, 2);
    token = strtok(NULL, s); //addr_

    // check how long the hex is
    uint8_t size = 0;
    char *hex_ptr = token + 2;
    while(hex_ptr != NULL || *hex_ptr != ' ' || *hex_ptr != '\n' || *hex_ptr != '\0')
    {
        size++;
        hex_ptr++;
    }

    memcpy(tokenAddr, &token[2], size); // copy addr from after 0x
    memcpy(baseAddr, &token[2], size - 3);

    // get the final 3 digits and divide them by 4 to get final address
    uint32_t addrVal; //= CLI_convertStrUint(&tokenAddr[0]);
    sscanf(&tokenAddr[0], "%x", &addrVal);
    uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
    addrVal = ((addrVal - baseAddrVal) / ADDRESS_CONVERT_RATIO);

    return addrVal;
}

//w 0x1a10601c 0x0003
//returns 0003
static CRS_retVal_t getVal(char *line, char *ret)
{
    char *token = NULL;
    char *sep = " ";

    token = MyStrTok(line, sep); // w_
    token = MyStrTok(NULL, sep); // 0x1a10601c_
    token = MyStrTok(NULL, sep);// 0x0003

    token += 2; // skip 0x

    uint8_t size = 0;
    char *hex_ptr = token + 2;
    while(hex_ptr != NULL || *hex_ptr != ' ' || *hex_ptr != '\n' || *hex_ptr != '\0')
    {
        size++;
        hex_ptr++;
    }

    memcpy(ret, &token[2], size);

    return CRS_SUCCESS;
}

static bool isInvalidAddr(char *line)
{
        uint32_t addrVal = getAddress(line);
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

static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber)
{
    uint32_t addrVal = getAddress(line);
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

    return (CRS_SUCCESS);
}

static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg)
{
    uint32_t addrVal = getAddress(line);
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

static CRS_retVal_t CheckLineSyntax(char *line, enum command_type cmd)
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
        if(line [strlen(line) - 1] != ':')
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
        char *command = MyStrTok(line, sep);// if_
        if (0 != memcmp(CMD_IF, command, strlen(command)))
        {
            return CRS_FAILURE;
        }
        command = MyStrTok(NULL, sep);//var1_
        command = MyStrTok(NULL, sep);//==_
        if (0 != memcmp("== ", command, strlen(command)))
        {
            return CRS_FAILURE;
        }
        command = MyStrTok(NULL, sep);//var2_
        command = MyStrTok(NULL, sep);//then_
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





static uint8_t is_delim(char c, const char *delim)
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

static char *MyStrTok(char *srcString,const char *delim)
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
        if(is_delim(*srcString, delim))
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
        if(is_delim(*srcString, delim))
        {
            *srcString = '\0';
            backup_string = srcString + 1;
            return ret;
        }

        srcString++;
    }
}




