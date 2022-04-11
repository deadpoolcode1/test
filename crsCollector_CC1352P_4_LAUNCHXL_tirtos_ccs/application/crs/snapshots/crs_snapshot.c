/*
 * crs_snapshot.c
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */

#include "crs_snapshot.h"

#define FILE_CACHE_SZ 300

#define LINE_SZ 50

#define RUN_NEXT_LINE_EV 0x1
#define STAR_RSP_EV 0x2
#define FINISHED_FILE_EV 0x4


static uint32_t gLineNumber = 1;
static char gFileToUpload[FILENAME_SZ] = { 0 };

static char gLineToSendArray[9][CRS_NVS_LINE_BYTES] = { 0 };
static uint32_t gLUT_line = 0;
static CRS_chipMode_t gMode = MODE_NATIVE;
static CRS_chipType_t gChipType = DIG;

static FPGA_cbFn_t gCbFn = NULL;

static uint16_t gSnapEvents = 0;

static char gStarRdRespLine[LINE_SZ] = { 0 };

static Semaphore_Handle collectorSem;

static char gFileContentCache[FILE_CACHE_SZ] = { 0 };
static uint32_t gFileContentCacheIdx = 0;

static void uploadSnapRawCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapSlaveCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapNativeCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapStarCb(const FPGA_cbArgs_t _cbArgs);

static CRS_retVal_t zeroLineToSendArray();
static CRS_retVal_t getPrevLine(char *line);

static CRS_retVal_t getNextLine(char *line);
static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
                                uint32_t numLines, char *respLine);

static void Nvs_uploadSnapStarCb(const FPGA_cbArgs_t _cbArgs);
static void finishedFileCb(const FPGA_cbArgs_t _cbArgs);

CRS_retVal_t SnapInit(void *sem)
{
    collectorSem = sem;

}

void Snap_process(void)
{
    CRS_retVal_t rspStatus;

    if (gSnapEvents & RUN_NEXT_LINE_EV)
    {
        char line[LINE_SZ] = { 0 };

        rspStatus = getNextLine(line);
        if (rspStatus == CRS_FAILURE)
        {
            if (gChipType == RF)
            {
                zeroLineToSendArray();
                Convert_applyLine(gLineToSendArray, gLUT_line, gMode);
                char lineToSend[100] = { 0 };
                flat2DArray(gLineToSendArray, 3, lineToSend);
                Fpga_writeMultiLine(lineToSend, finishedFileCb);
                Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
                return;
            }
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
                            Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
                            return;


        }

        zeroLineToSendArray();
        if (((strstr(line, "16b'")) || (strstr(line, "32b'"))))
        {
            Convert_rdParser(line, gLineToSendArray);
            char lineToSend[100] = { 0 };
            flat2DArray(gLineToSendArray, 2, lineToSend);
            Fpga_writeMultiLine(lineToSend, uploadSnapStarCb);
            Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
            return;

        }
        //uploadSnapNativeCb
        if (gChipType == RF)
        {
            rspStatus = Convert_readLineRf(line, gLineToSendArray, gLUT_line,
                                           gMode);
        }
        else if (gChipType == DIG)
        {
            rspStatus = Convert_readLineDig(line, gLineToSendArray, gMode);
        }
        else if (gChipType == UNKNOWN)
        {
            rspStatus = Convert_readLineFpga(line, gLineToSendArray);

//            strcpy(gLineToSendArray[0], line);
            rspStatus = CRS_SUCCESS;
        }

        if (rspStatus == CRS_NEXT_LINE)
        {
            Semaphore_post(collectorSem);

            return;
        }

        char lineToSend[100] = { 0 };
        int i = 0, numLines = 0;

        for (i = 0; i < 6; i++)
        {
            if (gLineToSendArray[i][0] == 0)
            {
                break;
            }
            numLines++;
        }

        flat2DArray(gLineToSendArray, numLines, lineToSend);
        Fpga_writeMultiLine(lineToSend, uploadSnapNativeCb);

        Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
        return;

//        Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    }

    if (gSnapEvents & STAR_RSP_EV)
    {

        char line[LINE_SZ] = { 0 };

        rspStatus = getPrevLine(line);
        if (rspStatus == CRS_FAILURE)
        {
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gSnapEvents, STAR_RSP_EV);
            return;
        }

        char starParsedLine[LINE_SZ] = { 0 };
        Convert_convertStar(line, gStarRdRespLine, starParsedLine);
        zeroLineToSendArray();

        if (gChipType == RF)
        {
            rspStatus = Convert_readLineRf(starParsedLine, gLineToSendArray, gLUT_line,
                                           gMode);
        }
        else if (gChipType == DIG)
        {
            rspStatus = Convert_readLineDig(starParsedLine, gLineToSendArray, gMode);
        }
        else if (gChipType == UNKNOWN)
        {
            rspStatus = Convert_readLineFpga(starParsedLine, gLineToSendArray);

//            strcpy(gLineToSendArray[0], line);
            rspStatus = CRS_SUCCESS;
        }

        if (rspStatus == CRS_NEXT_LINE)
        {
            Util_clearEvent(&gSnapEvents, STAR_RSP_EV);
            Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);

            Semaphore_post(collectorSem);

            return;
        }

        char lineToSend[100] = { 0 };
        int i = 0, numLines = 0;

        for (i = 0; i < 6; i++)
        {
            if (gLineToSendArray[i][0] == 0)
            {
                break;
            }
            numLines++;
        }

        flat2DArray(gLineToSendArray, numLines, lineToSend);
        Fpga_writeMultiLine(lineToSend, uploadSnapNativeCb);

        Util_clearEvent(&gSnapEvents, STAR_RSP_EV);
        return;

        Util_clearEvent(&gSnapEvents, STAR_RSP_EV);
    }

}

static CRS_retVal_t digRunNextLine()
{
    CRS_retVal_t rspStatus;
    char line[LINE_SZ] = { 0 };

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
        line[lineIdx] = gFileContentCache[gFileContentCacheIdx];
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

}

static void uploadSnapStarCb(const FPGA_cbArgs_t _cbArgs)
{
    char *line = _cbArgs.arg3;
    uint32_t size = _cbArgs.arg0;

    Util_setEvent(&gSnapEvents, STAR_RSP_EV);
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

static void uploadSnapNativeCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}
static void finishedFileCb(const FPGA_cbArgs_t _cbArgs)
{


    const FPGA_cbArgs_t cbArgs;
                gCbFn(cbArgs);
//                Util_clearEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
}

static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static void uploadSnapSlaveCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

CRS_retVal_t Snap_uploadSnapRf(char *filename, uint32_t rfAddr,
                               uint32_t LUTLineNum, CRS_chipMode_t chipMode,CRS_nameValue_t *nameVals,
                               FPGA_cbFn_t cbFunc)
{
    gCbFn = cbFunc;
    gLUT_line = LUTLineNum;
    gMode = chipMode;
    gChipType = RF;
    CLI_cliPrintf("\r\n");
    CLI_cliPrintf("\r\nin Snap_uploadSnapRf runing %s, lut line:0x%x", filename, LUTLineNum);

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;

    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    rspStatus = Nvs_readFile(gFileToUpload, gFileContentCache);
    if (rspStatus != CRS_SUCCESS)
    {
        return rspStatus;
    }
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);

    return rspStatus;

}

CRS_retVal_t Snap_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                                FPGA_cbFn_t cbFunc)
{
    gCbFn = cbFunc;
    gMode = chipMode;
    gChipType = DIG;
    CLI_cliPrintf("\r\n");

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;

    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    rspStatus = Nvs_readFile(gFileToUpload, gFileContentCache);
    if (rspStatus != CRS_SUCCESS)
    {
        return rspStatus;
    }
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);

    return rspStatus;

}

CRS_retVal_t Snap_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                FPGA_cbFn_t cbFunc)
{
    gCbFn = cbFunc;
    gMode = chipMode;
    gChipType = UNKNOWN;
    CLI_cliPrintf("\r\n");

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;

    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    rspStatus = Nvs_readFile(gFileToUpload, gFileContentCache);
    if (rspStatus != CRS_SUCCESS)
    {
        return rspStatus;
    }
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);

    return rspStatus;

}



CRS_retVal_t Snap_uploadSnapRaw(char *filename, CRS_chipMode_t chipMode,
                                FPGA_cbFn_t cbFunc)
{
    gCbFn = cbFunc;
    gMode = chipMode;
    gChipType = UNKNOWN;
    CLI_cliPrintf("\r\n");

    CRS_retVal_t rspStatus = CRS_SUCCESS;

    memcpy(gFileToUpload, filename, FILENAME_SZ);
    gFileContentCacheIdx = 0;

    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    Nvs_readFile(gFileToUpload, gFileContentCache);
    Util_setEvent(&gSnapEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);

    return rspStatus;

}




