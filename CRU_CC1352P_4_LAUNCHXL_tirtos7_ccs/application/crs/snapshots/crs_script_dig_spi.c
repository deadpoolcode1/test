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
#include "crs_script_dig_spi.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define FILE_CACHE_SZ 300

#define LINE_SZ 50

#define RUN_NEXT_LINE_EV 0x1
#define STAR_RSP_EV 0x2
#define FINISHED_FILE_EV 0x4
#define CHANGE_DIG_CHIP_EV 0x8


typedef struct scriptDigTraverser
{
    CRS_nameValue_t nameValues[NAME_VALUES_SZ];
    uint32_t digAddr;
    CRS_chipMode_t chipMode;
    CRS_chipType_t chipType;
    char fileToUpload[FILENAME_SZ];
    char *fileContentCache;
    uint32_t fileContentCacheIdx;
    char starRdRespLine[LINE_SZ];
    bool isFileDone;
    char lineToSendArray[9][CRS_NVS_LINE_BYTES];
}scriptDigTraverser_t;
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

//static FPGA_tmpCbFn_t gCbFn = NULL;

static uint16_t gDigEvents = 0;

static char gStarRdRespLine[LINE_SZ] = { 0 };

static Semaphore_Handle collectorSem;

static char *gFileContentCache = NULL;
static uint32_t gFileContentCacheIdx = 0;

static bool gIsFileDone = false;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void uploadSnapDigCb(void);
//static void uploadSnapSlaveCb(const FPGA_cbArgs_t _cbArgs);
//static void uploadSnapNativeCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapStarCb(scriptDigTraverser_t *fileTraverser, char *line);

static CRS_retVal_t zeroLineToSendArray(scriptDigTraverser_t *fileTraverser);
static CRS_retVal_t getPrevLine(scriptDigTraverser_t *fileTraverser, char *line);

static CRS_retVal_t getNextLine(scriptDigTraverser_t *fileTraverser, char *line);
//static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
//                                uint32_t numLines, char *respLine);

//static void finishedFileCb(const FPGA_cbArgs_t _cbArgs);

static CRS_retVal_t runLine(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runApplyCommand(char *line);
static CRS_retVal_t runStarCommand(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t incermentParam(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t addParam(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runSlashCommand(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runGotoCommand(scriptDigTraverser_t *fileTraverser, char *line); //expecting to accept 'goto label'
static CRS_retVal_t runIfCommand(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runPrintCommand(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runWCommand(char *line);
static CRS_retVal_t runRCommand(char *line);
static CRS_retVal_t runEwCommand(scriptDigTraverser_t *fileTraverser, char *line);
static CRS_retVal_t runErCommand(char *line);
static CRS_retVal_t getAddress(char *line, uint32_t *rsp);
static CRS_retVal_t getVal(char *line, char *rsp);
static CRS_retVal_t isNextLine(char *line, bool *rsp);
static void setDigClock(uint32_t time);
static void processDigTimeoutCallback(UArg a0);
static CRS_retVal_t addEndOfFlieSequence(scriptDigTraverser_t *fileTraverser);
static void uploadSnapRdDigCb(void);
static void changedDigChipCb(void);
static CRS_retVal_t runWrCommand(char *line);


static CRS_retVal_t runNextLine(scriptDigTraverser_t *fileTraverser);
static void changeDigChip(scriptDigTraverser_t *fileTraverser);
static CRS_retVal_t starRsp(scriptDigTraverser_t *fileTraverser);

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t DigSPI_init(void *sem)
{
    collectorSem = sem;
    digClkHandle = UtilTimer_construct(&digClkStrct, processDigTimeoutCallback,
                                       5, 0,
                                       false,
                                       0);
    return CRS_SUCCESS;
}

CRS_retVal_t DigSPI_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                               uint32_t chipAddr, CRS_nameValue_t *nameVals)
{

    if (Fpga_isOpen() == CRS_FAILURE)
    {
//        CLI_cliPrintf("\r\nOpen Fpga first");
////        const FPGA_cbArgs_t cbArgs = { 0 };
////        cbFunc(cbArgs);
//        cbFunc();
//        return CRS_FAILURE;

    }

    scriptDigTraverser_t fileTraverser = {0};
    fileTraverser.digAddr = chipAddr;
    fileTraverser.chipMode = chipMode;
    fileTraverser.chipType = DIG;
    fileTraverser.fileContentCache = NULL;
//    gDigAddr = chipNumber;
//    gCbFn = cbFunc;
//    gMode = chipMode;
//    gChipType = DIG;
    CLI_cliPrintf("\r\n");
    if (nameVals != NULL)
    {
        int i;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memcpy(fileTraverser.nameValues[i].name, nameVals[i].name, NAMEVALUE_NAME_SZ);
            fileTraverser.nameValues[i].value = nameVals[i].value;
        }
    }
    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(fileTraverser.fileToUpload, filename, FILENAME_SZ);
    fileTraverser.fileContentCacheIdx = 0;

//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    fileTraverser.fileContentCache = Nvs_readFileWithMalloc(filename);

//    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (fileTraverser.fileContentCache == NULL)
    {
        return CRS_FAILURE;
    }

    if (chipAddr != 0xff)
    {
//        Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
        changeDigChip(&fileTraverser);
    }
    runNextLine(&fileTraverser);
//    else
//    {
//        Util_setEvent(&gDigEvents, CHANGE_DIG_CHIP_EV);

//    }
    CRS_free(&fileTraverser.fileContentCache);
    return rspStatus;

}

CRS_retVal_t DigSPI_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                CRS_nameValue_t *nameVals)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
//        CLI_cliPrintf("\r\nOpen Fpga first");
////        const FPGA_cbArgs_t cbArgs = { 0 };
////        cbFunc(cbArgs);
//        cbFunc();
//        return CRS_SUCCESS;
//
    }
//    gCbFn = cbFunc;
    scriptDigTraverser_t fileTraverser = {0};
    fileTraverser.chipMode = chipMode;
    fileTraverser.chipType = UNKNOWN;
//    CLI_cliPrintf("\r\n");
    if (nameVals != NULL && nameVals[0].name[0] != 0)
    {
        int i;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memcpy(fileTraverser.nameValues[i].name, nameVals[i].name, NAMEVALUE_NAME_SZ);
            fileTraverser.nameValues[i].value = nameVals[i].value;
        }
    }
    CRS_LOG(CRS_DEBUG, "running file: %s", filename);

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(fileTraverser.fileToUpload, filename, FILENAME_SZ);
    fileTraverser.fileContentCacheIdx = 0;
    fileTraverser.isFileDone = false;
    fileTraverser.fileContentCache = NULL;
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    fileTraverser.fileContentCache = Nvs_readFileWithMalloc(filename);
    if (fileTraverser.fileContentCache == NULL)
    {
        return CRS_FAILURE;
    }
    runNextLine(&fileTraverser);
    CRS_free(&fileTraverser.fileContentCache);
    fileTraverser.fileContentCache = NULL;
//    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);

    return rspStatus;

}
void DigSPI_process(void)
{
    CRS_retVal_t rspStatus;
//
//    if (gDigEvents & RUN_NEXT_LINE_EV)
//    {
//        char line[LINE_SZ] = { 0 };
//
////        rspStatus = getNextLine(line);
//        if (rspStatus == CRS_FAILURE)
//        {
//            if (gIsFileDone == false && gChipType != UNKNOWN)
//            {
//                gIsFileDone = true;
////                addEndOfFlieSequence();
////                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
////                getNextLine(line);
//            }
//            else
//            {
//                gIsFileDone = false;
//                CRS_free(gFileContentCache);
//                gFileContentCache = NULL;
////                const FPGA_cbArgs_t cbArgs={0};
////                gCbFn(cbArgs);
////                gCbFn();
//                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//                return;
//            }
//
//        }
//
//        runLine(line);
//
//        Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//        return;
//
////        Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//    }
//
//    if (gDigEvents & STAR_RSP_EV)
//    {
//
//        char line[LINE_SZ] = { 0 };
//
//        rspStatus = getPrevLine(line);
//        if (rspStatus == CRS_FAILURE)
//        {
//            CRS_free(gFileContentCache);
//            gFileContentCache = NULL;
//
////            const FPGA_cbArgs_t cbArgs={0};
////            gCbFn(cbArgs);
////            gCbFn();
//            Util_clearEvent(&gDigEvents, STAR_RSP_EV);
//            return;
//        }
//
//        char starParsedLine[LINE_SZ] = { 0 };
//        Convert_convertStar(line, gStarRdRespLine, starParsedLine);
//        zeroLineToSendArray();
//
//        if (gChipType == DIG)
//        {
//            rspStatus = Convert_readLineDig(starParsedLine, gLineToSendArray,
//                                            gMode);
//        }
//        else if (gChipType == UNKNOWN)
//        {
//            rspStatus = Convert_readLineFpga(starParsedLine, gLineToSendArray);
//
////            strcpy(gLineToSendArray[0], line);
//            rspStatus = CRS_SUCCESS;
//        }
//
//        if (rspStatus == CRS_NEXT_LINE)
//        {
//            Util_clearEvent(&gDigEvents, STAR_RSP_EV);
//            Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//
//            Semaphore_post(collectorSem);
//
//            return;
//        }
//
//        Util_clearEvent(&gDigEvents, STAR_RSP_EV);
//        return;
//
//    }
//
//    //CHANGE_DIG_CHIP_EV
//    if (gDigEvents & CHANGE_DIG_CHIP_EV)
//    {
//        CRS_LOG(CRS_DEBUG, "\r\nin CHANGE_DIG_CHIP_EV runing");
//        char line[100] = { 0 };
//        sprintf(line, "wr 0xff 0x%x", gDigAddr);
////        Fpga_writeMultiLine(line, changedDigChipCb);
//        uint32_t rsp = 0;
//        Fpga_tmpWriteMultiLine(line, &rsp);
//        changedDigChipCb();
//        Util_clearEvent(&gDigEvents, CHANGE_DIG_CHIP_EV);
//    }

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static void changedDigChipCb(void)
{
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static CRS_retVal_t runLine(scriptDigTraverser_t *fileTraverser, char *line)
{

    CRS_retVal_t retVal = CRS_SUCCESS;
//    while (true)
//    {

//        CRS_LOG(CRS_DEBUG, "Running line: %s", line);
//        char rspLine[100] = { 0 };

        if (((strstr(line, "16b'")) || (strstr(line, "32b'"))))
        {
            runStarCommand(fileTraverser, line);
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
            runEwCommand(fileTraverser, line);
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
            runSlashCommand(fileTraverser, line);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "if", 2) == 0)
        {
            runIfCommand(fileTraverser, line);
            return CRS_SUCCESS;
        }
        else if (memcmp(line, "goto", 4) == 0)
        {
            runGotoCommand(fileTraverser, line);
            return CRS_SUCCESS;
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
            runPrintCommand(fileTraverser, line);
            return CRS_SUCCESS;

        }
        else if (strstr(line, "=") != NULL)
        {
            if (strstr(line, "+") == NULL)
            {
                addParam(fileTraverser, line);
            }
            else if (strstr(line, "(") == NULL)
            {
                incermentParam(fileTraverser, line);
            }
            return CRS_SUCCESS;
        }
//        memset(line, 0, 100);
//        retVal = getNextLine(fileTraverser, line);
//        if (retVal == CRS_FAILURE)
//        {
//            if (fileTraverser->isFileDone == false && gChipType != UNKNOWN)
//            {
//                fileTraverser->isFileDone = true;
//                addEndOfFlieSequence(fileTraverser);
//                //                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//                getNextLine(fileTraverser, line);
//            }
//            else
//            {
//                fileTraverser->isFileDone = false;
//
//                CRS_free(fileTraverser->fileContentCache);
//                fileTraverser->fileContentCache = NULL;
////                const FPGA_cbArgs_t cbArgs={0};
////                gCbFn(cbArgs);
////                gCbFn();
////                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
//                return CRS_SUCCESS;
//            }
//        }
//    }
        return CRS_SUCCESS;
}

static CRS_retVal_t runApplyCommand(char *line)
{
    char lineToSend[100] = { 0 };
    sprintf(lineToSend, "wr 0x50 0x000000\nwr 0x50 0x000001");
//    Fpga_writeMultiLine(lineToSend, uploadSnapDigCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(line, &rsp);
    uploadSnapDigCb();

    return CRS_SUCCESS;

}

static CRS_retVal_t runStarCommand(scriptDigTraverser_t *fileTraverser, char *line)
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

//    Fpga_writeMultiLine(lineToSend, uploadSnapStarCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(lineToSend, &rsp);
    uploadSnapStarCb(fileTraverser, lineToSend);

    return CRS_SUCCESS;

}

static CRS_retVal_t incermentParam(scriptDigTraverser_t *fileTraverser, char *line)
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
            if (memcmp(fileTraverser->nameValues[i].name, a, strlen(a)) == 0)
            {
                aInt = fileTraverser->nameValues[i].value;
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
            if (memcmp(fileTraverser->nameValues[i].name, b, strlen(b)) == 0)
            {
                bInt = fileTraverser->nameValues[i].value;
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
        if (memcmp(fileTraverser->nameValues[i].name, varName, NAMEVALUE_NAME_SZ) == 0)
        {
            fileTraverser->nameValues[i].value = result;
            break;
        }
        i++;
    }
    return CRS_SUCCESS;
}
static CRS_retVal_t addParam(scriptDigTraverser_t *fileTraverser, char *line)
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
        if (*(fileTraverser->nameValues[idx].name) == 0)
        {
            memcpy(fileTraverser->nameValues[idx].name, varName, NAMEVALUE_NAME_SZ);
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
            if (memcmp(fileTraverser->nameValues[i].name, varValue, NAMEVALUE_NAME_SZ) == 0)
            {
//                CLI_cliPrintf("gNameValues[idx].value: %d\r\ngNameValues[i].value: %d\r\n",gNameValues[idx].value,gNameValues[i].value);
                fileTraverser->nameValues[idx].value = fileTraverser->nameValues[i].value;
                break;
            }
            i++;
        }
    }
    else
    {
        fileTraverser->nameValues[idx].value = strtol(varValue, NULL, 10);
    }
    return CRS_SUCCESS;
}
static CRS_retVal_t runSlashCommand(scriptDigTraverser_t *fileTraverser, char *line)
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
            if (memcmp(fileTraverser->nameValues[i].name, varName, strlen(varName)) == 0)
            {
                isExistParam = true;
            }
            i++;
        }
        if (isExistParam == false)
        {
            while (1)
            {

                if (*(fileTraverser->nameValues[idx].name) == 0)
                {
                    memcpy(fileTraverser->nameValues[idx].name, varName, NAMEVALUE_NAME_SZ);
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
            fileTraverser->nameValues[idx].value = strtol(varValue, NULL, 10);
            i = 0;
        }
//        CLI_cliPrintf("\r\nname:%s\r\nvalue:%s\r\n", varName, varValue);
    }
    return CRS_SUCCESS;
}

static CRS_retVal_t runGotoCommand(scriptDigTraverser_t *fileTraverser, char *line) //expecting to accept 'goto label'
{
    char *ptr = line;
    ptr += 5;        //skip 'goto '
    char label[10] = { 0 };
    strcat(label, "\n");
    strcat(label, ptr);
    strcat(label, ":");
    char *ptrResp = strstr(fileTraverser->fileContentCache, label);
    ptrResp += strlen(label) + 1;
    fileTraverser->fileContentCacheIdx = ptrResp - fileTraverser->fileContentCache;

    return CRS_SUCCESS;
}

static CRS_retVal_t runIfCommand(scriptDigTraverser_t *fileTraverser, char *line)
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
        if (memcmp(fileTraverser->nameValues[i].name, param, NAMEVALUE_NAME_SZ) == 0)
        {
            break;
        }
        i++;
    }
    ptr += 6;        //skip ' then '
    strcat(label, ptr);
    if (fileTraverser->nameValues[i].value == comparedValInt)
    {
        runGotoCommand(fileTraverser, label);
    }
    return CRS_SUCCESS;
}

static CRS_retVal_t runPrintCommand(scriptDigTraverser_t *fileTraverser, char *line)
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
            if (memcmp(fileTraverser->nameValues[i].name, param, NAMEVALUE_NAME_SZ) == 0)
            {
                break;
            }
            i++;
        }
        CLI_cliPrintf(" %d", fileTraverser->nameValues[i].value);
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


//    Fpga_writeMultiLine(wr_converted, uploadSnapDigCb);
    uint32_t returnVal = 0;
    Fpga_tmpWriteMultiLine(wr_converted, &returnVal);
//    uploadSnapDigCb();

    return CRS_SUCCESS;

}

static CRS_retVal_t runWrCommand(char *line)
{

//    Fpga_writeMultiLine(line, uploadSnapDigCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(line, &rsp);
//    uploadSnapDigCb();

    return CRS_SUCCESS;

}

static CRS_retVal_t runRCommand(char *line)
{
    uint32_t addr;
    getAddress(line, &addr);
    char r_converted[100] = { 0 };
    sprintf(r_converted, "wr 0x51 0x%x0000\nrd 0x51", addr);

//    Fpga_writeMultiLine(r_converted, uploadSnapRdDigCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(r_converted, &rsp);
//    uploadSnapRdDigCb();

    return CRS_SUCCESS;
}

static CRS_retVal_t runEwCommand(scriptDigTraverser_t *fileTraverser, char *line)
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
            if (memcmp(fileTraverser->nameValues[i].name, token, strlen(token)) == 0)
            {
                sprintf(lineToSend + strlen(lineToSend), " 0x%x",
                        fileTraverser->nameValues[i].value);
                break;
            }
            i++;
        }
    }
    else
    {
        strcat(lineToSend, token);
    }

//    Fpga_writeMultiLine(lineToSend, uploadSnapDigCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(lineToSend, &rsp);
//    uploadSnapDigCb();

    return CRS_SUCCESS;
}

static CRS_retVal_t runErCommand(char *line)
{
    char lineToSend[100] = { 0 };
    memcpy(lineToSend, line, 100);

    lineToSend[0] = 'r';
    lineToSend[1] = 'd';

//    Fpga_writeMultiLine(lineToSend, uploadSnapRdDigCb);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(lineToSend, &rsp);
//    uploadSnapRdDigCb();

    return CRS_SUCCESS;
}

static CRS_retVal_t addEndOfFlieSequence(scriptDigTraverser_t *fileTraverser)
{
    CRS_free(&fileTraverser->fileContentCache);
    fileTraverser->fileContentCache = CRS_malloc(700);
    if (fileTraverser->fileContentCache == NULL)
    {
        CRS_LOG(CRS_ERR, "Malloc failed");
        return CRS_FAILURE;
    }

    char *applyCommand = "\nwr 0x50 0x000000\nwr 0x50 0x000000";
    char *delayCommand = "\ndelay 1000";
    char *tmp2 = "\nwr 0x50 0x47fc42";
    char *tmp3 = "\nwr 0x50 0x47fc43";

    memset(fileTraverser->fileContentCache, 0, 700);

    memcpy(fileTraverser->fileContentCache, &applyCommand[1], strlen(applyCommand) - 1);
    strcat(fileTraverser->fileContentCache, delayCommand);

    strcat(fileTraverser->fileContentCache, tmp2);
    strcat(fileTraverser->fileContentCache, applyCommand);
    strcat(fileTraverser->fileContentCache, delayCommand);

    strcat(fileTraverser->fileContentCache, tmp3);
    strcat(fileTraverser->fileContentCache, applyCommand);
    strcat(fileTraverser->fileContentCache, delayCommand);

    strcat(fileTraverser->fileContentCache, tmp2);
    strcat(fileTraverser->fileContentCache, applyCommand);
    strcat(fileTraverser->fileContentCache, delayCommand);

    strcat(fileTraverser->fileContentCache, tmp3);
    strcat(fileTraverser->fileContentCache, applyCommand);

    fileTraverser->fileContentCacheIdx = 0;
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

static CRS_retVal_t getPrevLine(scriptDigTraverser_t *fileTraverser, char *line)
{
//    static char gFileContentCache[FILE_CACHE_SZ] = { 0 };
//    static uint32_t gFileContentCacheIdx = 0;
    if (fileTraverser->fileContentCacheIdx == 0)
    {
        return CRS_FAILURE;
    }
    fileTraverser->fileContentCacheIdx--;
    while (fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] == '\n')
    {
        if (fileTraverser->fileContentCacheIdx == 0)
        {
            break;
        }
        fileTraverser->fileContentCacheIdx--;
    }

    while (fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] != '\n')
    {
        if (fileTraverser->fileContentCacheIdx == 0)
        {
            break;
        }
        fileTraverser->fileContentCacheIdx--;
    }
    if (fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] == '\n')
    {
        fileTraverser->fileContentCacheIdx++;
    }
    return getNextLine(fileTraverser, line);

}

static CRS_retVal_t getNextLine(scriptDigTraverser_t *fileTraverser, char *line)
{
//    static char gFileContentCache[FILE_CACHE_SZ] = { 0 };
//    static uint32_t gFileContentCacheIdx = 0;

    while (fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] == '\n')
    {
        fileTraverser->fileContentCacheIdx++;
    }

    if (fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] == 0)
    {
        return CRS_FAILURE;
    }

    char *endOfLine = strchr(&fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx], '\n');
    if (endOfLine == NULL)
    {
        strcpy(line, &fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx]);
        fileTraverser->fileContentCacheIdx += strlen(line);
        return CRS_SUCCESS;
    }

    uint32_t lineIdx = 0;

    while (&fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx] != endOfLine)
    {
        if (lineIdx <= 95)
        {
            line[lineIdx] = fileTraverser->fileContentCache[fileTraverser->fileContentCacheIdx];
        }
        lineIdx++;
        fileTraverser->fileContentCacheIdx++;
    }
    fileTraverser->fileContentCacheIdx++;
    return CRS_SUCCESS;
}

static CRS_retVal_t zeroLineToSendArray(scriptDigTraverser_t *fileTraverser)
{
    int i = 0;
    for (i = 0; i < 9; i++)
    {
        memset(fileTraverser->lineToSendArray[i], 0, CRS_NVS_LINE_BYTES);
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

static void uploadSnapStarCb(scriptDigTraverser_t *fileTraverser, char *line)
{
//    uint32_t size = _cbArgs.arg0;

//    Util_setEvent(&gDigEvents, STAR_RSP_EV);
    memset(fileTraverser->starRdRespLine, 0, LINE_SZ);

    int tmpLine_idx = 0;
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
            fileTraverser->starRdRespLine[tmpLine_idx] = line[counter];
            tmpLine_idx++;
            counter++;
            continue;
        }

        if (line[counter] == '\r' || line[counter] == '\n')
        {
            isNumber = false;
        }

        if (isNumber == true)
        {
            fileTraverser->starRdRespLine[tmpLine_idx] = line[counter];
            tmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }
    //    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
    starRsp(fileTraverser);
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

static void uploadSnapDigCb(void)
{
    Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static void uploadSnapRdDigCb(void)
{
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

static CRS_retVal_t runNextLine(scriptDigTraverser_t *fileTraverser)
{

    while(true)
    {

      char line[LINE_SZ] = { 0 };
      memset(line, 0, LINE_SZ);

      CRS_retVal_t rspStatus = getNextLine(fileTraverser, line);
      if (rspStatus == CRS_FAILURE)
      {
          if (fileTraverser->isFileDone == false && fileTraverser->chipType != UNKNOWN)
          {
              fileTraverser->isFileDone = true;
              addEndOfFlieSequence(fileTraverser);
//                Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
              getNextLine(fileTraverser, line);
          }
          else
          {
              fileTraverser->isFileDone = false;
//              CRS_free(fileTraverser->fileContentCache);
//              fileTraverser->fileContentCache = NULL;
//                const FPGA_cbArgs_t cbArgs={0};
//                gCbFn(cbArgs);
//                gCbFn();
//              Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
              return CRS_SUCCESS;
          }

      }

      runLine(fileTraverser, line);
    }


//        Util_clearEvent(&gDigEvents, RUN_NEXT_LINE_EV);
}

static CRS_retVal_t starRsp(scriptDigTraverser_t *fileTraverser)
{
    char line[LINE_SZ] = { 0 };

     CRS_retVal_t rspStatus = getPrevLine(fileTraverser, line);
     if (rspStatus == CRS_FAILURE)
     {
         CRS_free(&fileTraverser->fileContentCache);
         fileTraverser->fileContentCache = NULL;

//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
//            gCbFn();
         return CRS_FAILURE;
     }

     char starParsedLine[LINE_SZ] = { 0 };
     Convert_convertStar(line, fileTraverser->starRdRespLine, starParsedLine);
     zeroLineToSendArray(fileTraverser);

     if (fileTraverser->chipType == DIG)
     {
         rspStatus = Convert_readLineDig(starParsedLine,fileTraverser->lineToSendArray,
                                         fileTraverser->chipMode);
     }
     else if (fileTraverser->chipType == UNKNOWN)
     {
         rspStatus = Convert_readLineFpga(starParsedLine, fileTraverser->lineToSendArray);

//            strcpy(gLineToSendArray[0], line);
         rspStatus = CRS_SUCCESS;
     }

     if (rspStatus == CRS_NEXT_LINE)
     {
//         Util_clearEvent(&gDigEvents, STAR_RSP_EV);
//         Util_setEvent(&gDigEvents, RUN_NEXT_LINE_EV);


         return CRS_SUCCESS;
     }

//     Util_clearEvent(&gDigEvents, STAR_RSP_EV);
     return CRS_SUCCESS;


}

static void changeDigChip(scriptDigTraverser_t *fileTraverser)
{
    char line[100] = { 0 };
    sprintf(line, "wr 0xff 0x%x\r", fileTraverser->digAddr);
    uint32_t rsp = 0;
    Fpga_tmpWriteMultiLine(line, &rsp);
}
