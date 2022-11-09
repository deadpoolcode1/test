/*
 * crs_script_dig.c
 *
 *  Created on: 7 Feb 2022
 *      Author: epc_4
 */

/*
 * crs_snapshot.c
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_script_dig.h"
#include "config_parsing.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define FILE_CACHE_SZ 300

#define LINE_SZ 50

#define RUN_NEXT_LINE_EV 0x1
#define STAR_RSP_EV 0x2
#define FINISHED_FILE_EV 0x4
#define CHANGE_DIG_CHIP_EV 0x8

/******************************************************************************
 Local variables
 *****************************************************************************/

static CRS_nameValue_t gNameValues[NAME_VALUES_SZ];

static Clock_Struct digClkStrct;
static Clock_Handle digClkHandle;

static uint32_t gDigAddr = 0;

//static uint32_t gLineNumber = 1;
static char gFileToUpload[FILENAME_SZ] = { 0 };

static char gLineToSendArray[9][CRS_NVS_LINE_BYTES] = { 0 };
//static uint32_t gLUT_line = 0;
static CRS_chipMode_t gMode = MODE_NATIVE;
static CRS_chipType_t gChipType = DIG;

static FPGA_cbFn_t gCbFn = NULL;

static uint16_t gDigEvents = 0;

static char gStarRdRespLine[LINE_SZ] = { 0 };

static Semaphore_Handle collectorSem;

static char *gFileContentCache = NULL;
static uint32_t gFileContentCacheIdx = 0;

static bool gIsFileDone = false;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs);
//static void uploadSnapSlaveCb(const FPGA_cbArgs_t _cbArgs);
//static void uploadSnapNativeCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapStarCb(const FPGA_cbArgs_t _cbArgs);

static CRS_retVal_t zeroLineToSendArray();
static CRS_retVal_t getPrevLine(char *line);

static CRS_retVal_t getNextLine(char *line);
//static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
//                                uint32_t numLines, char *respLine);

//static void finishedFileCb(const FPGA_cbArgs_t _cbArgs);

static CRS_retVal_t runLine(char *line);
static CRS_retVal_t runApplyCommand(char *line);
static CRS_retVal_t runStarCommand(char *line);
static CRS_retVal_t incermentParam(char *line);
static CRS_retVal_t addParam(char *line);
static CRS_retVal_t runSlashCommand(char *line);
static CRS_retVal_t runGotoCommand(char *line); //expecting to accept 'goto label'
static CRS_retVal_t runIfCommand(char *line);
static CRS_retVal_t runPrintCommand(char *line);
static CRS_retVal_t runWCommand(char *line);
static CRS_retVal_t runRCommand(char *line);
static CRS_retVal_t runEwCommand(char *line);
static CRS_retVal_t runErCommand(char *line);
static CRS_retVal_t getAddress(char *line, uint32_t *rsp);
static CRS_retVal_t getVal(char *line, char *rsp);
static CRS_retVal_t isNextLine(char *line, bool *rsp);
static void setDigClock(uint32_t time);
static void processDigTimeoutCallback(UArg a0);
static CRS_retVal_t addEndOfFlieSequence();
static void uploadSnapRdDigCb(const FPGA_cbArgs_t _cbArgs);
static void changedDigChipCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t runWrCommand(char *line);

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t DigInit(void *sem)
{
    collectorSem = sem;
    digClkHandle = UtilTimer_construct(&digClkStrct, processDigTimeoutCallback,
                                       5, 0,
                                       false,
                                       0);
    return CRS_SUCCESS;
}

CRS_retVal_t DIG_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                               uint32_t chipNumber, CRS_nameValue_t *nameVals,
                               FPGA_cbFn_t cbFunc)
{

    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;

    }
    CRS_LOG(CRS_DEBUG, "running file: %s", filename);
    gDigAddr = chipNumber;
    gCbFn = cbFunc;
    gMode = chipMode;
    gChipType = DIG;
    CLI_cliPrintf("\r\n");
    if (nameVals != NULL)
    {
        int i;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memcpy(gNameValues[i].name, nameVals[i].name, NAMEVALUE_NAME_SZ);
            gNameValues[i].value = nameVals[i].value;
        }
    }
    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;

//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
        gIsConfigOk = false;
        return CRS_FAILURE;
    }

    if (chipNumber == 0xff)
    {
        Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    }
    else
    {
        Util_setEvent(&gDigEvents, CHANGE_DIG_CHIP_EV);
    }
    Semaphore_post(collectorSem);

    return rspStatus;

}

CRS_retVal_t DIG_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                CRS_nameValue_t *nameVals, FPGA_cbFn_t cbFunc)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;

    }
    gCbFn = cbFunc;
    gMode = chipMode;
    gChipType = UNKNOWN;
    CLI_cliPrintf("\r\n");
    if (nameVals != NULL)
    {
        int i;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memcpy(gNameValues[i].name, nameVals[i].name, NAMEVALUE_NAME_SZ);
            gNameValues[i].value = nameVals[i].value;
        }
    }
    CRS_LOG(CRS_DEBUG, "running file: %s", filename);

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;
    gIsFileDone = false;

//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
        gIsConfigOk = false;
        return CRS_FAILURE;
    }
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);

    return rspStatus;

}
void DIG_process(void)
{
    CRS_retVal_t rspStatus;

    if (gDigEvents & RUN_NEXT_LINE_EV)
    {
        char line[LINE_SZ] = { 0 };

        rspStatus = getNextLine(line);
        if (rspStatus == CRS_FAILURE)
        {
            if (gIsFileDone == false && gChipType != UNKNOWN)
            {
                gIsFileDone = true;
                addEndOfFlieSequence();
//                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
                getNextLine(line);
            }
            else
            {
                gIsFileDone = false;
                CRS_free(gFileContentCache);
                gFileContentCache = NULL;
                const FPGA_cbArgs_t cbArgs={0};
                gCbFn(cbArgs);
                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
                return;
            }

        }

        runLine(line);

        Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
        return;

//        Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    }

    if (gDigEvents & STAR_RSP_EV)
    {

        char line[LINE_SZ] = { 0 };

        rspStatus = getPrevLine(line);
        if (rspStatus == CRS_FAILURE)
        {
            CRS_free(gFileContentCache);
            gFileContentCache = NULL;

            const FPGA_cbArgs_t cbArgs={0};
            gCbFn(cbArgs);
            Util_clearEvent(&gDigEvents, STAR_RSP_EV);
            return;
        }

        char starParsedLine[LINE_SZ] = { 0 };
        Convert_convertStar(line, gStarRdRespLine, starParsedLine);
        zeroLineToSendArray();

        if (gChipType == DIG)
        {
            rspStatus = Convert_readLineDig(starParsedLine, gLineToSendArray,
                                            gMode);
        }
        else if (gChipType == UNKNOWN)
        {
            rspStatus = Convert_readLineFpga(starParsedLine, gLineToSendArray);

//            strcpy(gLineToSendArray[0], line);
            rspStatus = CRS_SUCCESS;
        }

        if (rspStatus == CRS_NEXT_LINE)
        {
            Util_clearEvent(&gDigEvents, STAR_RSP_EV);
            Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);

            Semaphore_post(collectorSem);

            return;
        }

        Util_clearEvent(&gDigEvents, STAR_RSP_EV);
        return;

    }

    //CHANGE_DIG_CHIP_EV
    if (gDigEvents & CHANGE_DIG_CHIP_EV)
    {
        CRS_LOG(CRS_DEBUG, "\r\nin CHANGE_DIG_CHIP_EV runing");
        char line[100] = { 0 };
        sprintf(line, "wr 0xff 0x%x", gDigAddr);
        Fpga_writeMultiLineNoPrint(line, changedDigChipCb);
        Util_clearEvent(&gDigEvents, CHANGE_DIG_CHIP_EV);
    }

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static void changedDigChipCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static CRS_retVal_t runLine(char *line)
{

    CRS_retVal_t retVal = CRS_SUCCESS;
    while (true)
    {

        CRS_LOG(CRS_DEBUG, "Running line: %s", line);
//        char rspLine[100] = { 0 };

        if (((strstr(line, "16b'")) || (strstr(line, "32b'"))))
        {
            runStarCommand(line);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "w ", 2) == 0)
        {
            retVal = runWCommand(line);
            if (retVal != CRS_NEXT_LINE)
            {
                return CRS_SUCCESS;
            }
        }
        else if (memcmp(line, "wr ", 2) == 0)
        {
            retVal = runWrCommand(line);
            return CRS_SUCCESS;

        }
        else if (memcmp(line, "r ", 2) == 0)
        {
            retVal = runRCommand(line);
            if (retVal != CRS_NEXT_LINE)
            {
                return CRS_SUCCESS;
            }
        }
        else if (memcmp(line, "ew", 2) == 0)
        {
            runEwCommand(line);
//            CLI_cliPrintf("DIG after ew!!! %s", line);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "er", 2) == 0)
        {
            runErCommand(line);
            return CRS_SUCCESS;

        }
        else if (memcmp(line, "//@@", 4) == 0)
        {
            runSlashCommand(line);
        }
        else if (memcmp(line, "if", 2) == 0)
        {
            runIfCommand(line);
        }
        else if (memcmp(line, "goto", 4) == 0)
        {
            runGotoCommand(line);
        }
        else if (memcmp(line, "apply", 5) == 0)
        {
            runApplyCommand(line);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "delay", 5) == 0)
        {
            uint32_t time = strtoul(line + 6, NULL, 10);
            setDigClock(time);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "print", 5) == 0)
        {
            runPrintCommand(line);
//        return CRS_SUCCESS;

        }
        else if (strstr(line, "=") != NULL)
        {
            if (strstr(line, "+") == NULL)
            {
                addParam(line);
            }
            else if (strstr(line, "(") == NULL)
            {
                incermentParam(line);
            }

        }
        memset(line, 0, 100);
        retVal = getNextLine(line);
        if (retVal == CRS_FAILURE)
        {
            if (gIsFileDone == false && gChipType != UNKNOWN)
            {
                gIsFileDone = true;
                addEndOfFlieSequence();
                //                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
                getNextLine(line);
            }
            else
            {
                gIsFileDone = false;

                CRS_free(gFileContentCache);
                gFileContentCache = NULL;
                const FPGA_cbArgs_t cbArgs={0};
                gCbFn(cbArgs);
                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
                return CRS_SUCCESS;
            }
        }
    }
}

static CRS_retVal_t runApplyCommand(char *line)
{
    char lineToSend[100] = { 0 };
    sprintf(lineToSend, "wr 0x50 0x000000\nwr 0x50 0x000001");
    Fpga_writeMultiLineNoPrint(lineToSend, uploadSnapDigCb);
    return CRS_SUCCESS;

}

static CRS_retVal_t runStarCommand(char *line)
{
    uint32_t addrVal;
    char lineToSend[100] = { 0 };

    if (memcmp(line, "ew", 2) == 0)
    {
        addrVal = CLI_convertStrUint(&line[5]);
        sprintf(lineToSend, "wr 0x51 0x%x\nrd 0x51", addrVal);
    }
    else
    {
        getAddress(line, &addrVal);
        sprintf(lineToSend, "wr 0x51 0x%x\nrd 0x51", addrVal);

    }

    Fpga_writeMultiLineNoPrint(lineToSend, uploadSnapStarCb);

    return CRS_SUCCESS;

}

static CRS_retVal_t incermentParam(char *line)
{
    char *ptr = line;
    char varName[NAMEVALUE_NAME_SZ] = { 0 };
    char a[NAMEVALUE_NAME_SZ] = { 0 };
    int32_t aInt = 0;
    char b[NAMEVALUE_NAME_SZ] = { 0 };
    int32_t bInt = 0;
    int32_t result = 0;
//    char varValue[10] = { 0 };
    int i = 0;
    while (*ptr != ' ')
    {
        varName[i] = *ptr;
        i++;
        ptr++;
    }
    ptr += 3;        //skip ' = '
    i = 0;
    while (*ptr != ' ')
    {
        a[i] = *ptr;
        i++;
        ptr++;
    }
    ptr += 3;        //skip ' + '
    i = 0;
    while (*ptr)
    {
        b[i] = *ptr;
        i++;
        ptr++;
    }

    if (!isdigit(*a))
    {
        i = 0;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, a, strlen(a)) == 0)
            {
                aInt = gNameValues[i].value;
                break;
            }
            i++;
        }
    }
    else
    {
        aInt = strtol(a, NULL, 10);
    }
    if (!isdigit(*b))
    {
        i = 0;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, b, strlen(b)) == 0)
            {
                bInt = gNameValues[i].value;
                break;
            }
            i++;
        }
    }
    else
    {
        bInt = strtol(b, NULL, 10);
    }

    result = aInt + bInt;
    i = 0;
    while (i < NAME_VALUES_SZ)
    {
        if (memcmp(gNameValues[i].name, varName, NAMEVALUE_NAME_SZ) == 0)
        {
            gNameValues[i].value = result;
            break;
        }
        i++;
    }
    return CRS_SUCCESS;
}
static CRS_retVal_t addParam(char *line)
{
    char *ptr = line;
    char varName[NAMEVALUE_NAME_SZ] = { 0 };
    char varValue[NAMEVALUE_NAME_SZ] = { 0 };
    int i = 0;
    while (*ptr != ' ')
    {
        varName[i] = *ptr;
        i++;
        ptr++;
    }
    ptr += 3;        //skip ' = '
    int idx = 0;
    while (1)
    {
        if (*(gNameValues[idx].name) == 0)
        {
            memcpy(gNameValues[idx].name, varName, NAMEVALUE_NAME_SZ);
            break;
        }
        idx++;
    }
    i = 0;
    while (*ptr)
    {
        varValue[i] = *ptr;
        i++;
        ptr++;
    }
    if (!isdigit(*varValue))
    {
        i = 0;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, varValue, NAMEVALUE_NAME_SZ) == 0)
            {
//                CLI_cliPrintf("gNameValues[idx].value: %d\r\ngNameValues[i].value: %d\r\n",gNameValues[idx].value,gNameValues[i].value);
                gNameValues[idx].value = gNameValues[i].value;
                break;
            }
            i++;
        }
    }
    else
    {
        gNameValues[idx].value = strtol(varValue, NULL, 10);
    }
    return CRS_SUCCESS;
}
static CRS_retVal_t runSlashCommand(char *line)
{

    char *ptr = line;
    char varName[NAMEVALUE_NAME_SZ] = { 0 };
    char varValue[NAMEVALUE_NAME_SZ] = { 0 };
    int i = 0;
    while (*ptr != ' ')
    {
        ptr++;
    }
    ptr++;
    if (memcmp(ptr, "param", 5) == 0)
    {
        ptr += 6;        //skip 'param '
        while (*ptr != ',')
        {
            varName[i] = *ptr;
            i++;
            ptr++;
        }
        int idx = 0;
        int i = 0;
        ;
        bool isExistParam = false;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, varName, strlen(varName)) == 0)
            {
                isExistParam = true;
            }
            i++;
        }
        if (isExistParam == false)
        {
            while (1)
            {

                if (*(gNameValues[idx].name) == 0)
                {
                    memcpy(gNameValues[idx].name, varName, NAMEVALUE_NAME_SZ);
                    break;
                }
                idx++;
            }
            i = 0;
            ptr++;
            while (*ptr != ',')
            {
                varValue[i] = *ptr;
                i++;
                ptr++;
            }
            gNameValues[idx].value = strtol(varValue, NULL, 10);
            i = 0;
        }
//        CLI_cliPrintf("\r\nname:%s\r\nvalue:%s\r\n", varName, varValue);
    }
    return CRS_SUCCESS;
}

static CRS_retVal_t runGotoCommand(char *line) //expecting to accept 'goto label'
{
    char *ptr = line;
    ptr += 5;        //skip 'goto '
    char label[10] = { 0 };
    strcat(label, "\n");
    strcat(label, ptr);
    strcat(label, ":");
    char *ptrResp = strstr(gFileContentCache, label);
    ptrResp += strlen(label) + 1;
    gFileContentCacheIdx = ptrResp - gFileContentCache;
    return CRS_SUCCESS;
}

static CRS_retVal_t runIfCommand(char *line)
{
    char param[NAMEVALUE_NAME_SZ] = { 0 };
    char comparedVal[10] = { 0 };
    char label[20] = { 0 };
    strcat(label, "goto ");
    int32_t comparedValInt = 0;
    char *ptr = line;
    ptr += 3;        //skip 'if '
    int i = 0;
    while (*ptr != ' ')
    {
        param[i] = *ptr;
        ptr++;
        i++;
    }
    i = 0;
    ptr += 4;        //skip ' == '
    while (*ptr != ' ')
    {
        comparedVal[i] = *ptr;
        ptr++;
        i++;
    }
    comparedValInt = strtol(comparedVal, NULL, 10);
    i = 0;
    while (1)
    {
        if (memcmp(gNameValues[i].name, param, NAMEVALUE_NAME_SZ) == 0)
        {
            break;
        }
        i++;
    }
    ptr += 6;        //skip ' then '
    strcat(label, ptr);
    if (gNameValues[i].value == comparedValInt)
    {
        runGotoCommand(label);
    }
    return CRS_SUCCESS;
}

static CRS_retVal_t runPrintCommand(char *line)
{
    char lineTemp[50] = { 0 };
    strcat(lineTemp, line);
    char *ptr = lineTemp;
    char param[NAMEVALUE_NAME_SZ] = { 0 };
    ptr += 7;        //skip 'print "'
    while (*ptr != '"')
    {
        CLI_cliPrintf("%c", *ptr);
        ptr++;
    }
    ptr += 2;        //skip '" '
    int i = 0;
    while (*ptr)
    {
        param[i] = *ptr;
        i++;
        ptr++;
    }
    i = 0;
    if (*param)
    {
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, param, NAMEVALUE_NAME_SZ) == 0)
            {
                break;
            }
            i++;
        }
        CLI_cliPrintf(" %d", gNameValues[i].value);
    }
    CLI_cliPrintf("\r\n");
//ptr+=2;//skip '" '
    return CRS_SUCCESS;
}

static CRS_retVal_t runWCommand(char *line)
{
    bool rsp = false;

    //if its next line
    isNextLine(line, &rsp);
    if (rsp == true)
    {
        return CRS_NEXT_LINE;
    }

    uint32_t addr;
    getAddress(line, &addr);
    char val[20] = { 0 };
    getVal(line, val);

    char wr_converted[30] = { 0 };
    memcpy(wr_converted, "wr 0x50 0x", strlen("wr 0x50 0x"));
    char addrStr[5] = { 0 };
    sprintf(addrStr, "%x", addr);

    strcat(wr_converted, addrStr);
    strcat(wr_converted, val);

    Fpga_writeMultiLineNoPrint(wr_converted, uploadSnapDigCb);

    return CRS_SUCCESS;

}

static CRS_retVal_t runWrCommand(char *line)
{

    Fpga_writeMultiLineNoPrint(line, uploadSnapDigCb);

    return CRS_SUCCESS;

}

static CRS_retVal_t runRCommand(char *line)
{
    uint32_t addr;
    getAddress(line, &addr);
    char r_converted[100] = { 0 };
    sprintf(r_converted, "wr 0x51 0x%x0000\nrd 0x51", addr);

    Fpga_writeMultiLineNoPrint(r_converted, uploadSnapRdDigCb);

    return CRS_SUCCESS;
}

static CRS_retVal_t runEwCommand(char *line)
{
    char lineToSend[100] = { 0 };
    char lineTemp[100] = { 0 };
    memcpy(lineTemp, line, 100);
//    memcpy(lineToSend, line, 100);
//ew addr param
    lineToSend[0] = 'w';
    lineToSend[1] = 'r';
    lineToSend[2] = ' ';
    const char s[2] = " ";
    char *token;
    token = strtok(lineTemp, s); //wr
    token = strtok(NULL, s); //addr
    strcat(lineToSend, token);
    token = strtok(NULL, s); //param or val
    int i = 0;
    lineToSend[strlen(lineToSend)] = ' ';
    //TODO: verify with michael how do we set params in files.
    if (*token == '_')
    {
        i = 0;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, token, strlen(token)) == 0)
            {
                sprintf(lineToSend + strlen(lineToSend), " 0x%x",
                        gNameValues[i].value);
                break;
            }
            i++;
        }
    }
    else
    {
        strcat(lineToSend, token);
    }

    Fpga_writeMultiLineNoPrint(lineToSend, uploadSnapDigCb);
    return CRS_SUCCESS;
}

static CRS_retVal_t runErCommand(char *line)
{
    char lineToSend[100] = { 0 };
    memcpy(lineToSend, line, 100);

    lineToSend[0] = 'r';
    lineToSend[1] = 'd';

    Fpga_writeMultiLineNoPrint(lineToSend, uploadSnapRdDigCb);
    return CRS_SUCCESS;
}

static CRS_retVal_t addEndOfFlieSequence()
{
    CRS_free(gFileContentCache);
    gFileContentCache = CRS_malloc(700);
    if (gFileContentCache == NULL)
    {
        CRS_LOG(CRS_ERR, "Malloc failed");
        return CRS_FAILURE;
    }

    char *applyCommand = "\nwr 0x50 0x000000\nwr 0x50 0x000000";
    char *delayCommand = "\ndelay 1000";
    char *tmp2 = "\nwr 0x50 0x47fc42";
    char *tmp3 = "\nwr 0x50 0x47fc43";

    memset(gFileContentCache, 0, 700);

    memcpy(gFileContentCache, &applyCommand[1], strlen(applyCommand) - 1);
    strcat(gFileContentCache, delayCommand);

    strcat(gFileContentCache, tmp2);
    strcat(gFileContentCache, applyCommand);
    strcat(gFileContentCache, delayCommand);

    strcat(gFileContentCache, tmp3);
    strcat(gFileContentCache, applyCommand);
    strcat(gFileContentCache, delayCommand);

    strcat(gFileContentCache, tmp2);
    strcat(gFileContentCache, applyCommand);
    strcat(gFileContentCache, delayCommand);

    strcat(gFileContentCache, tmp3);
    strcat(gFileContentCache, applyCommand);

    gFileContentCacheIdx = 0;
    return CRS_SUCCESS;

}
//w 0x1a10601c 0x0003
//returns 1c/4
static CRS_retVal_t getAddress(char *line, uint32_t *rsp)
{
    const char s[2] = " ";
    char *token;
    /* get the first token */
    char tokenMode[5] = { 0 }; //w r ew er
    char tokenAddr[15] = { 0 }; //0x1a10601c
//    char tokenValue[10] = { 0 }; //0x0003
    char baseAddr[15] = { 0 };
    memset(baseAddr, '0', 8);

    char tmpReadLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(tmpReadLine, line, CRS_NVS_LINE_BYTES);
    token = strtok((tmpReadLine), s); //w or r
    memcpy(tokenMode, token, 2);
    token = strtok(NULL, s); //addr
    memcpy(tokenAddr, &token[2], 8);
    memcpy(baseAddr, &token[2], 5);

    uint32_t addrVal; //= CLI_convertStrUint(&tokenAddr[0]);
    sscanf(&tokenAddr[0], "%x", &addrVal);
    uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
    addrVal = ((addrVal - baseAddrVal) / 4);

    *rsp = addrVal;
    return CRS_SUCCESS;

}

//w 0x1a10601c 0x0003
//returns 0003
static CRS_retVal_t getVal(char *line, char *rsp)
{
    const char s[2] = " ";
    char *token;
    /* get the first token */
    char tokenMode[5] = { 0 }; //w r ew er
    char tokenAddr[15] = { 0 }; //0x1a10601c
//    char tokenValue[10] = { 0 }; //0x0003
    char baseAddr[15] = { 0 };
    memset(baseAddr, '0', 8);

    char tmpReadLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(tmpReadLine, line, CRS_NVS_LINE_BYTES);
    token = strtok((tmpReadLine), s); //w or r
    memcpy(tokenMode, token, 2);
    token = strtok(NULL, s); //addr
    memcpy(tokenAddr, &token[2], 8);
    memcpy(baseAddr, &token[2], 5);

    uint32_t addrVal; //= CLI_convertStrUint(&tokenAddr[0]);
    sscanf(&tokenAddr[0], "%x", &addrVal);
    uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
    addrVal = ((addrVal - baseAddrVal) / 4);

    token = strtok(NULL, s); // value
    memcpy(rsp, &token[2], 4);

    return CRS_SUCCESS;

}

static CRS_retVal_t isNextLine(char *line, bool *rsp)
{
    uint32_t addrVal = 0;
    getAddress(line, &addrVal);
    if (addrVal < 0x8 || addrVal > 0x4b)
    {

        *rsp = true;
        return (CRS_NEXT_LINE);
    }

    *rsp = false;

    return (CRS_SUCCESS);

}

static CRS_retVal_t getPrevLine(char *line)
{
//    static char gFileContentCache[FILE_CACHE_SZ] = { 0 };
//    static uint32_t gFileContentCacheIdx = 0;
    if (gFileContentCacheIdx == 0)
    {
        return CRS_FAILURE;
    }
    gFileContentCacheIdx--;
    while (gFileContentCache[gFileContentCacheIdx] == '\n')
    {
        if (gFileContentCacheIdx == 0)
        {
            break;
        }
        gFileContentCacheIdx--;
    }

    while (gFileContentCache[gFileContentCacheIdx] != '\n')
    {
        if (gFileContentCacheIdx == 0)
        {
            break;
        }
        gFileContentCacheIdx--;
    }
    if (gFileContentCache[gFileContentCacheIdx] == '\n')
    {
        gFileContentCacheIdx++;
    }
    return getNextLine(line);

}

static CRS_retVal_t getNextLine(char *line)
{
//    static char gFileContentCache[FILE_CACHE_SZ] = { 0 };
//    static uint32_t gFileContentCacheIdx = 0;

    while (gFileContentCache[gFileContentCacheIdx] == '\n')
    {
        gFileContentCacheIdx++;
    }

    if (gFileContentCache[gFileContentCacheIdx] == 0)
    {
        return CRS_FAILURE;
    }

    char *endOfLine = strchr(&gFileContentCache[gFileContentCacheIdx], '\n');
    if (endOfLine == NULL)
    {
        strcpy(line, &gFileContentCache[gFileContentCacheIdx]);
        gFileContentCacheIdx += strlen(line);
        return CRS_SUCCESS;
    }

    uint32_t lineIdx = 0;

    while (&gFileContentCache[gFileContentCacheIdx] != endOfLine)
    {
        if (lineIdx <= 95)
        {
            line[lineIdx] = gFileContentCache[gFileContentCacheIdx];
        }
        lineIdx++;
        gFileContentCacheIdx++;
    }
    gFileContentCacheIdx++;
    return CRS_SUCCESS;
}

static CRS_retVal_t zeroLineToSendArray()
{
    int i = 0;
    for (i = 0; i < 9; i++)
    {
        memset(gLineToSendArray[i], 0, CRS_NVS_LINE_BYTES);
    }
    return CRS_SUCCESS;
}

//static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
//                                uint32_t numLines, char *respLine)
//{
//    int i = 0;
//
//    strcpy(respLine, lines[0]);
//
//    for (i = 1; i < numLines; i++)
//    {
//        respLine[strlen(respLine)] = '\n';
//        strcat(respLine, lines[i]);
//    }
//    return CRS_SUCCESS;
//}

static void uploadSnapStarCb(const FPGA_cbArgs_t _cbArgs)
{
    char *line = _cbArgs.arg3;
//    uint32_t size = _cbArgs.arg0;

    Util_setEvent(&gDigEvents, STAR_RSP_EV);
    memset(gStarRdRespLine, 0, LINE_SZ);

    int gTmpLine_idx = 0;
    int counter = 0;
    bool isNumber = false;
    bool isFirst = true;
    while (memcmp(&line[counter], "AP>", 3) != 0)
    {
        if (line[counter] == '0' && line[counter + 1] == 'x')
        {
            if (isFirst == true)
            {
                counter++;
                isFirst = false;
                continue;

            }
            isNumber = true;
            gStarRdRespLine[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;
        }

        if (line[counter] == '\r' || line[counter] == '\n')
        {
            isNumber = false;
        }

        if (isNumber == true)
        {
            gStarRdRespLine[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }
    //    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);

    Semaphore_post(collectorSem);
}

//static void uploadSnapNativeCb(const FPGA_cbArgs_t _cbArgs)
//{
//    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//    Semaphore_post(collectorSem);
//}
//static void finishedFileCb(const FPGA_cbArgs_t _cbArgs)
//{
//
//    const FPGA_cbArgs_t cbArgs;
//    gCbFn(cbArgs);
////                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//}

static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static void uploadSnapRdDigCb(const FPGA_cbArgs_t _cbArgs)
{
    CLI_cliPrintf("\r\n%s", _cbArgs.arg3);
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

//static void uploadSnapSlaveCb(const FPGA_cbArgs_t _cbArgs)
//{
//    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//    Semaphore_post(collectorSem);
//}

static void processDigTimeoutCallback(UArg a0)
{
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);

    Semaphore_post(collectorSem);
}

static void setDigClock(uint32_t time)
{
    /* Stop the Tracking timer */
    if (UtilTimer_isActive(&digClkStrct) == true)
    {
        UtilTimer_stop(&digClkStrct);
    }

    if (time)
    {
        /* Setup timer */
        UtilTimer_setTimeout(digClkHandle, time);
        UtilTimer_start(&digClkStrct);
    }
}

