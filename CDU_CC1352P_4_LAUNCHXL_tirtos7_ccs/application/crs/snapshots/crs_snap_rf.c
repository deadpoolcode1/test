/*
 * crs_snap_rf.c
 *
 *  Created on: 18 Jan 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_snap_rf.h"
#include "application/util_timer.h"
#include <ctype.h>
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define READ_NEXT_REG_EV 0x1
#define READ_NEXT_GLOBAL_REG_EV 0x2
#define FINISHED_FILE_EV 0x4
#define CHANGE_ACTIVE_LINE_EV 0x8
#define START_UPLOAD_FILE_EV 0x10
#define WRITE_NEXT_LUT_EV 0x20
#define CHANGE_RF_CHIP_EV 0x40
#define LUT_REG_NUM 8
#define NUM_LUTS 4
#define NUM_GLOBAL_REG 4
#define FILE_CACHE_SZ 4096
#define LINE_SZ 50
/******************************************************************************
 Local variables
 *****************************************************************************/
static Clock_Struct rfClkStruct;
static Clock_Handle rfClkHandle;
static uint32_t gRegIdx = 0;
static uint32_t gLutIdx = 0;
static uint32_t gGlobalIdx = 0;
static uint16_t gLineMatrix[NUM_LUTS][LUT_REG_NUM] = { 0 };
static uint16_t gGlobalReg[NUM_GLOBAL_REG] = { 0 };
static char gLineToSendArray[9][CRS_NVS_LINE_BYTES] = { 0 };
static uint32_t gRfAddr = 0;
static uint32_t gRFline = 0;
static CRS_chipMode_t gMode = MODE_NATIVE;
static FPGA_cbFn_t gCbFn = NULL;
static uint16_t gRFEvents = 0;
static char gLutRegRdResp[LINE_SZ] = { 0 };
static Semaphore_Handle collectorSem;

static bool gIsInTheMiddleOfTheFile = false;

static char *gFileContentCache = NULL;
static uint32_t gFileContentCacheIdx = 0;

static CRS_nameValue_t gNameValues[NAME_VALUES_SZ];
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void changedActiveLineCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
                                uint32_t numLines, char *respLine);
static void readLutRegCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t initRfSnapValues();
static CRS_retVal_t readLutReg();
static CRS_retVal_t readGlobalReg();
static void readGlobalRegCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t getPrevLine(char *line);
static CRS_retVal_t getNextLine(char *line);
static CRS_retVal_t runFile();
static CRS_retVal_t getVal(char *line, char *rsp);
static CRS_retVal_t isGlobal(char *line, bool *rsp);
static CRS_retVal_t isNextLine(char *line, bool *rsp);
static CRS_retVal_t getAddress(char *line, uint32_t *rsp);
static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg);
static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber);
static CRS_retVal_t runStarCommand(char *line, char *rspLine);
static CRS_retVal_t runWCommand(char *line);
static CRS_retVal_t runRCommand(char *line);
static CRS_retVal_t runEwCommand(char *line);
static CRS_retVal_t runErCommand(char *line);
static CRS_retVal_t addParam(char *line);
static CRS_retVal_t incermentParam(char *line);
static CRS_retVal_t runSlashCommand(char *line);
static CRS_retVal_t runIfCommand(char *line);
static CRS_retVal_t runGotoCommand(char *line);
static CRS_retVal_t runPrintCommand(char *line);
static CRS_retVal_t initNameValues();
static CRS_retVal_t writeLutToFpga(uint32_t lutNumber);
static CRS_retVal_t convertLutRegToAddrStr(uint32_t regIdx, char *addr);
static void writeLutCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t convertLutRegDataToStr(uint32_t regIdx, uint32_t lutNumber,
                                           char *data);
static CRS_retVal_t writeGlobalsToFpga();
static void writeGlobalsCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t convertGlobalRegDataToStr(uint32_t regIdx, char *data);
static CRS_retVal_t convertGlobalRegToAddrStr(uint32_t regIdx, char *addr);
static void processRfTimeoutCallback(UArg a0);
static void setRfClock(uint32_t time);
static void changedRfChipCb(const FPGA_cbArgs_t _cbArgs);

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t RF_init(void *sem)
{
    collectorSem = sem;

    rfClkHandle = UtilTimer_construct(&rfClkStruct, processRfTimeoutCallback, 5,
                                      0,
                                      false,
                                      0);
}

CRS_retVal_t RF_uploadSnapRf(char *filename, uint32_t rfAddr,
                             uint32_t RfLineNum, CRS_chipMode_t chipMode,
                             CRS_nameValue_t *nameVals, FPGA_cbFn_t cbFunc)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;

    }

    initRfSnapValues();

    gCbFn = cbFunc;
    gRFline = RfLineNum;
    gMode = chipMode;
    gRfAddr = rfAddr;
    if (nameVals != NULL)
    {
        int i;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memcpy(gNameValues[i].name, nameVals[i].name, NAMEVALUE_NAME_SZ);
            gNameValues[i].value = nameVals[i].value;
        }
    }
    CLI_cliPrintf("\r\n");
    CRS_LOG(CRS_DEBUG, " runing %s, lut line:0x%x", filename, RfLineNum);

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
        return CRS_FAILURE;
    }

//filling up local rf line
    if (rfAddr == 0xff)
    {
        Util_setEvent(&gRFEvents, CHANGE_ACTIVE_LINE_EV);
    }
    else
    {
        Util_setEvent(&gRFEvents, CHANGE_RF_CHIP_EV);
    }

    Semaphore_post(collectorSem);

    return rspStatus;
}

void RF_process(void)
{
    if (gRFEvents & READ_NEXT_REG_EV)
    {

        //last lut only 28 bits
        if (gLutIdx == 3)
        {
            //finished reading

            if (gRegIdx == 2)
            {
                Util_clearEvent(&gRFEvents, READ_NEXT_REG_EV);
                Util_setEvent(&gRFEvents, READ_NEXT_GLOBAL_REG_EV);
                Semaphore_post(collectorSem);
                return;
            }
        }

        if (gRegIdx == LUT_REG_NUM)
        {
            gRegIdx = 0;
            gLutIdx++;
            readLutReg();

        }
        else
        {
            readLutReg();

        }

        Util_clearEvent(&gRFEvents, READ_NEXT_REG_EV);
    }

    if (gRFEvents & CHANGE_ACTIVE_LINE_EV)
    {
        CRS_LOG(CRS_DEBUG, "in CHANGE_ACTIVE_LINE_EV runing");
        char line[100] = { 0 };
        sprintf(line, "wr 0xa 0x%x", gRFline);
        Fpga_writeMultiLine(line, changedActiveLineCb);
        Util_clearEvent(&gRFEvents, CHANGE_ACTIVE_LINE_EV);
    }

    if (gRFEvents & CHANGE_RF_CHIP_EV)
    {
        CRS_LOG(CRS_DEBUG, "in CHANGE_RF_CHIP_EV runing");
        char line[100] = { 0 };
        sprintf(line, "wr 0xff 0x%x", gRfAddr);
        Fpga_writeMultiLine(line, changedRfChipCb);
        Util_clearEvent(&gRFEvents, CHANGE_RF_CHIP_EV);
    }

    if (gRFEvents & START_UPLOAD_FILE_EV)
    {
//        sprintf("wr 0xa 0x%x", gRFline);
//        Fpga_writeMultiLine(line, changedActiveLineCb);
        Util_clearEvent(&gRFEvents, START_UPLOAD_FILE_EV);
        CRS_retVal_t rsp = runFile();
        if (rsp == CRS_SUCCESS)
        {
            gRegIdx = 0;
            gLutIdx = 0;
            gGlobalIdx = 0;
            writeLutToFpga(gLutIdx);
        }

    }

    if (gRFEvents & READ_NEXT_GLOBAL_REG_EV)
    {
        if (gGlobalIdx == NUM_GLOBAL_REG)
        {
            Util_clearEvent(&gRFEvents, READ_NEXT_GLOBAL_REG_EV);
            Util_setEvent(&gRFEvents, START_UPLOAD_FILE_EV);
            Semaphore_post(collectorSem);
            return;
        }
        else
        {
            readGlobalReg();

        }

        Util_clearEvent(&gRFEvents, READ_NEXT_GLOBAL_REG_EV);
    }

    if (gRFEvents & WRITE_NEXT_LUT_EV)
    {
        if (gLutIdx < 4)
        {
            writeLutToFpga(gLutIdx);
        }
        else
        {
            writeGlobalsToFpga();

        }
        Util_clearEvent(&gRFEvents, WRITE_NEXT_LUT_EV);
    }
    //WRITE_NEXT_LUT_EV
//    FINISHED_FILE_EV
    if (gRFEvents & FINISHED_FILE_EV)
    {
        if (gIsInTheMiddleOfTheFile == true)
        {
            gIsInTheMiddleOfTheFile = false;
            Util_setEvent(&gRFEvents, START_UPLOAD_FILE_EV);
            Semaphore_post(collectorSem);
        }
        else
        {
            CRS_free(gFileContentCache);
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
        }

        Util_clearEvent(&gRFEvents, FINISHED_FILE_EV);
    }

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t runFile()
{

    char line[100] = { 0 };
    while (getNextLine(line) == CRS_SUCCESS)
    {
        CRS_LOG(CRS_DEBUG, "Running line: %s", line);
        char rspLine[100] = { 0 };

        if (((strstr(line, "16b'")) || (strstr(line, "32b'"))))
        {
            runStarCommand(line, rspLine);

        }
        else if (memcmp(line, "w ", 2) == 0)
        {
            runWCommand(line);
        }
        else if (memcmp(line, "r ", 2) == 0)
        {
            runRCommand(line);
        }
        else if (memcmp(line, "ew", 2) == 0)
        {
            runEwCommand(line);
        }
        else if (memcmp(line, "er", 2) == 0)
        {
            runErCommand(line);
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
            gIsInTheMiddleOfTheFile = true;
            return CRS_SUCCESS;

//            runWCommand("wr 0x50 0x000000");
//            runWCommand("wr 0x50 0x000001");
        }
        else if (memcmp(line, "delay", 5) == 0)
        {
            uint32_t time = strtoul(line + 6, NULL, 10);
            setRfClock(time);
            return CRS_FAILURE;
        }
        else if (memcmp(line, "print", 5) == 0)
        {
            runPrintCommand(line);
        }
        else if (strstr(line, "=") != NULL && strstr(line, "ret") == NULL)
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

        //isGlobal
        //w 0x1a10601c 0x8888
        //convert the line to the lut num and lut reg. and get the val to put into there.
        //matrix[lut][reg] = val
        memset(line, 0, 100);

    }
    return CRS_SUCCESS;

}

static CRS_retVal_t runStarCommand(char *line, char *rspLine)
{
    bool rsp = false;
    //if its a global reg
    isGlobal(line, &rsp);
    if (rsp == true)
    {
        uint32_t addrVal = 0;
        char *valLine = &line[17];
        getAddress(line, &addrVal);
        uint32_t regVal = gGlobalReg[addrVal - 0x3f];

        int i = 0;
        for (i = 0; i < 16; i++)
        {

            if (valLine[i] == '*')
            {
                continue;
            }
            else if (valLine[i] == '1')
            {
                regVal |= (1 << (15 - i));
            }
            else if (valLine[i] == '0')
            {
                regVal &= ~(1 << (15 - i));
            }
        }

        gGlobalReg[addrVal - 0x3f] = regVal;

        return CRS_SUCCESS;
    }

    //if its next line
    isNextLine(line, &rsp);
    if (rsp == true)
    {
        return CRS_SUCCESS;
    }

    uint32_t lutNumber;
    uint32_t lutReg;
    getLutNumberFromLine(line, &lutNumber);
    getLutRegFromLine(line, &lutReg);

    uint32_t addrVal = 0;
    char *valLine = &line[17];
    getAddress(line, &addrVal);
    uint32_t regVal = gLineMatrix[lutNumber][lutReg];

    int i = 0;
    for (i = 0; i < 16; i++)
    {

        if (valLine[i] == '*')
        {
            continue;
        }
        else if (valLine[i] == '1')
        {
            regVal |= (1 << (15 - i));
        }
        else if (valLine[i] == '0')
        {
            regVal &= ~(1 << (15 - i));
        }
    }

    gLineMatrix[lutNumber][lutReg] = regVal;
}

static CRS_retVal_t runWCommand(char *line)
{
    bool rsp = false;
    //if its a global reg
    isGlobal(line, &rsp);
    if (rsp == true)
    {
        uint32_t addrVal = 0;
        char val[20] = { 0 };
        getAddress(line, &addrVal);
        getVal(line, val);
        gGlobalReg[addrVal - 0x3f] = strtoul(val, NULL, 16);
        return CRS_SUCCESS;
    }

    //if its next line
    isNextLine(line, &rsp);
    if (rsp == true)
    {
        return CRS_SUCCESS;
    }

    uint32_t lutNumber;
    uint32_t lutReg;
    getLutNumberFromLine(line, &lutNumber);
    getLutRegFromLine(line, &lutReg);

    char val[20] = { 0 };
    getVal(line, val);

    gLineMatrix[lutNumber][lutReg] = strtoul(val, NULL, 16);
}

static CRS_retVal_t initNameValues()
{
    int i;
    for (i = 0; i < NAME_VALUES_SZ; ++i)
    {
        memset(gNameValues[i].name, 0, NAMEVALUE_NAME_SZ);
    }
}

static CRS_retVal_t runRCommand(char *line)
{

}

static CRS_retVal_t runEwCommand(char *line)
{
    char lineToSend[100] = { 0 };
    char lineTemp[100] = { 0 };
    memcpy(lineTemp, line, 100);
    //    memcpy(lineToSend, line, 100);

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

}
static CRS_retVal_t runErCommand(char *line)
{

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
    char varValue[10] = { 0 };
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

}

static CRS_retVal_t writeGlobalsToFpga()
{

    char lines[600] = { 0 };
    int x = 0;
    for (x = 0; x < 4; x++)
    {
        char addr[11] = { 0 };
        char val[11] = { 0 };

        convertGlobalRegToAddrStr(x, addr);
        convertGlobalRegDataToStr(x, val);
        sprintf(&lines[strlen(lines)], "wr 0x50 0x%s%s\n", addr, val);
    }
    lines[strlen(lines) - 1] = 0;

    Fpga_writeMultiLine(lines, writeGlobalsCb);

}

static void writeGlobalsCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gRFEvents, FINISHED_FILE_EV);
    Semaphore_post(collectorSem);
}

static CRS_retVal_t convertGlobalRegDataToStr(uint32_t regIdx, char *data)
{
    char valStr[10] = { 0 };
    memset(valStr, '0', 4);
    uint16_t val = gGlobalReg[regIdx];
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

static CRS_retVal_t writeLutToFpga(uint32_t lutNumber)
{
    char *startSeq = "wr 0x50 0x430000\nwr 0x50 0x000001\nwr 0x50 0x000000";
    char endSeq[100] = { 0 };
    sprintf(endSeq, "\nwr 0x50 0x430%x0%x\nwr 0x50 0x000001\nwr 0x50 0x000000",
            gRFline, (1 << lutNumber));
    char lines[600] = { 0 };
    memcpy(lines, startSeq, strlen(startSeq));
    int x = 0;
    for (x = 0; x < 8; x++)
    {
        char addr[11] = { 0 };
        char val[11] = { 0 };

        convertLutRegToAddrStr(x, addr);
        convertLutRegDataToStr(x, lutNumber, val);
        sprintf(&lines[strlen(lines)], "\nwr 0x50 0x%s%s", addr, val);
    }
    sprintf(&lines[strlen(lines)], "%s", endSeq);

    Fpga_writeMultiLine(lines, writeLutCb);

    return (CRS_SUCCESS);

}
static void writeLutCb(const FPGA_cbArgs_t _cbArgs)
{
    gLutIdx++;
    Util_setEvent(&gRFEvents, WRITE_NEXT_LUT_EV);
    Semaphore_post(collectorSem);
}
static CRS_retVal_t convertLutRegDataToStr(uint32_t regIdx, uint32_t lutNumber,
                                           char *data)
{
    char valStr[10] = { 0 };
    memset(valStr, '0', 4);
    uint16_t val = gLineMatrix[lutNumber][regIdx];
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

static void changedActiveLineCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gRFEvents, READ_NEXT_REG_EV);
    Semaphore_post(collectorSem);
}

static void changedRfChipCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gRFEvents, CHANGE_ACTIVE_LINE_EV);
    Semaphore_post(collectorSem);
}

static CRS_retVal_t initRfSnapValues()
{
    gIsInTheMiddleOfTheFile = false;
    initNameValues();

    gCbFn = NULL;
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);

    gFileContentCacheIdx = 0;

    gRegIdx = 0;
    gLutIdx = 0;
    gGlobalIdx = 0;
    int i = 0;
    for (i = 0; i < NUM_LUTS; ++i)
    {
        memset(gLineMatrix[i], 0, LUT_REG_NUM);
    }
    memset(gGlobalReg, 0, NUM_GLOBAL_REG);
}

static CRS_retVal_t isGlobal(char *line, bool *rsp)
{
    uint32_t addrVal = 0;
    getAddress(line, &addrVal);
    if (addrVal >= 0x3F && addrVal <= 0x42)
    {
        *rsp = true;

        return (CRS_SUCCESS);
    }
    *rsp = false;

    return (CRS_SUCCESS);

}

static CRS_retVal_t isNextLine(char *line, bool *rsp)
{
    uint32_t addrVal = 0;
    getAddress(line, &addrVal);
    if (((addrVal < 0x8 || addrVal > 0x27) && (addrVal < 0x3F || addrVal > 0x42)))
    {

        *rsp = true;
        return (CRS_NEXT_LINE);
    }

    if (addrVal == 0x25)
    {
        *rsp = true;

        return (CRS_NEXT_LINE);
    }
    *rsp = false;

    return (CRS_SUCCESS);

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
    char tokenValue[10] = { 0 }; //0x0003
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
    memcpy(tokenValue, &token[2], 4);

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
    char tokenValue[10] = { 0 }; //0x0003
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

    token = strtok(NULL, s); // value | param
    //TODO: verify with michael how do we set params in files.
    if (*token == '_')    //if token is param
    {
        int i = 0;
        while (i < NAME_VALUES_SZ)
        {
            if (memcmp(gNameValues[i].name, token, strlen(token)) == 0)
            {
                sprintf(rsp, "%x", gNameValues[i].value);
                break;
            }
            i++;
        }
    }
    else    //if token is a value
    {
        memcpy(rsp, &token[2], 4);
    }

    return CRS_SUCCESS;

}

static CRS_retVal_t getLutNumberFromLine(char *line, uint32_t *lutNumber)
{
    uint32_t addrVal = 0;
    getAddress(line, &addrVal);
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

static CRS_retVal_t getLutRegFromLine(char *line, uint32_t *lutReg)
{
    uint32_t addrVal = 0;
    getAddress(line, &addrVal);
    if (addrVal == 0x26)
    {
        *lutReg = 0;
        return (CRS_SUCCESS);
    }
    else if (addrVal == 0x27)
    {
        *lutReg = 1;
        return (CRS_SUCCESS);
    }
    switch (addrVal % 8)
    {
    case 0:
        *lutReg = 0;
        break;
    case 1:
        *lutReg = 1;
        break;
    case 2:
        *lutReg = 2;
        break;
    case 3:
        *lutReg = 3;
        break;
    case 4:
        *lutReg = 4;
        break;
    case 5:
        *lutReg = 5;
        break;
    case 6:
        *lutReg = 6;
        break;
    case 7:
        *lutReg = 7;
        break;
    }
    return (CRS_SUCCESS);

}

static CRS_retVal_t readLutReg()
{
//b'wr 0x50 0x390062\r'
//b'wr 0x50 0x000001\r'
//b'wr 0x50 0x000000\r'
//b'wr 0x51 0x510000\r'
//b'rd 0x51\r'

    char addr[15] = { 0 };
    char convertedRdResp[2][CRS_NVS_LINE_BYTES] = { 0 };
//    Convert_wr(addr, gMode, gRfAddr, convertedRdResp);

    char lines[9][CRS_NVS_LINE_BYTES] = { 0 };
    sprintf(lines[0], "wr 0x50 0x3900%x%x", gRegIdx, gLutIdx);
    memcpy(lines[1], "wr 0x50 0x000001", strlen("wr 0x50 0x000001"));
    memcpy(lines[2], "wr 0x50 0x000000", strlen("wr 0x50 0x000000"));
    memcpy(lines[3], "wr 0x51 0x510000", strlen("wr 0x51 0x510000"));
    memcpy(lines[4], "rd 0x51", strlen("rd 0x51"));

    char line[200] = { 0 };
    flat2DArray(lines, 5, line);
    Fpga_writeMultiLine(line, readLutRegCb);

}

static CRS_retVal_t readGlobalReg()
{
//b'wr 0x50 0x390062\r'
//b'wr 0x50 0x000001\r'
//b'wr 0x50 0x000000\r'
//b'wr 0x51 0x510000\r'
//b'rd 0x51\r'

//from 0x3f to 0x42
    char lines[9][CRS_NVS_LINE_BYTES] = { 0 };
    sprintf(lines[0], "wr 0x51 0x%x0000", gGlobalIdx + 0x3f);
    memcpy(lines[1], "rd 0x51", strlen("rd 0x51"));

    char line[200] = { 0 };
    flat2DArray(lines, 2, line);
    Fpga_writeMultiLine(line, readGlobalRegCb);

}

static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
                                uint32_t numLines, char *respLine)
{
    int i = 0;

    strcpy(respLine, lines[0]);

    for (i = 1; i < numLines; i++)
    {
        respLine[strlen(respLine)] = '\n';
        strcat(respLine, lines[i]);
    }
    return CRS_SUCCESS;

}

static CRS_retVal_t convertRfRegLut(uint32_t regIdx, uint32_t lutIdx,
                                    char *resp)
{
//    wr 0x50 0x3900  0   0
//                  reg  lut
//wr 0x51 0x39

}

static void readLutRegCb(const FPGA_cbArgs_t _cbArgs)
{
    char *line = _cbArgs.arg3;
    uint32_t size = _cbArgs.arg0;

    memset(gLutRegRdResp, 0, LINE_SZ);

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
            gLutRegRdResp[gTmpLine_idx] = line[counter];
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
            gLutRegRdResp[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }

    gLineMatrix[gLutIdx][gRegIdx] = strtoul(&gLutRegRdResp[6], NULL, 16);
    gRegIdx++;

//    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
    Util_setEvent(&gRFEvents, READ_NEXT_REG_EV);

    Semaphore_post(collectorSem);
}

static void readGlobalRegCb(const FPGA_cbArgs_t _cbArgs)
{
    char *line = _cbArgs.arg3;
    uint32_t size = _cbArgs.arg0;

    memset(gLutRegRdResp, 0, LINE_SZ);

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
            gLutRegRdResp[gTmpLine_idx] = line[counter];
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
            gLutRegRdResp[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }

    gGlobalReg[gGlobalIdx] = strtoul(&gLutRegRdResp[6], NULL, 16);
    gGlobalIdx++;

//    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
    Util_setEvent(&gRFEvents, READ_NEXT_GLOBAL_REG_EV);

    Semaphore_post(collectorSem);
}

static void processRfTimeoutCallback(UArg a0)
{
    Util_setEvent(&gRFEvents, START_UPLOAD_FILE_EV);

    Semaphore_post(collectorSem);
}

static void setRfClock(uint32_t time)
{
    /* Stop the Tracking timer */
    if (UtilTimer_isActive(&rfClkStruct) == true)
    {
        UtilTimer_stop(&rfClkStruct);
    }

    if (time)
    {
        /* Setup timer */
        UtilTimer_setTimeout(rfClkHandle, time);
        UtilTimer_start(&rfClkStruct);
    }
}