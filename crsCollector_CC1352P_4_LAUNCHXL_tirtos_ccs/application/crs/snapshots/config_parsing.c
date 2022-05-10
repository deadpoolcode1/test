/*
 * config_parsing.c
 *
 *  Created on: 5 ????? 2022
 *      Author: cellium
 */
#include "config_parsing.h"
#include <ti/sysbios/knl/Semaphore.h>
#include "application/crs/crs_fpga.h"
#include "crs_convert_snapshot.h"
#include "crs_multi_snapshots.h"

#define TEMP_SZ 200
#define DISCSEQ_SZ 4
#define PACKGES_SZ 5
#define FILE_CACHE_SZ 100
#define NONE 255

#define RUN_NEXT_LINE_EV 0x1
#define FINISHED_DISCOVERY_EV 0x2
#define FINISHED_SINGLE_LINE_EV 0x4

static CRS_invLine_t gInvLineStrct = { 0 };
static char gDiscRdRespLine[TEMP_SZ] = { 0 };

static char gInvName[30] = { 0 };
static char gDiscExpectVal[EXPECTEDVAL_SZ] = { 0 };
static Semaphore_Handle collectorSem;
static uint16_t gConfigEvents = 0;

static char *gFileContentCache;
static uint32_t gFileContentCacheIdx = 0;

static uint32_t gInvLineNumber = 0;

static bool gIsSingleLine = false;

//static crs_fileInfo_t gFileInfos[NAME_VALUES_SZ] = { 0 };

static FPGA_cbFn_t gCbFn = NULL;

static CRS_retVal_t parseBox(char *buff, crs_box_t *respStructBox);
static CRS_retVal_t parseInv(char *buff, crs_inv_t respStructInv);
static CRS_retVal_t parsePackages(char *buff, crs_package_t *respStructPackage);
static CRS_retVal_t parseFileInfo(char *buff,
                                  crs_fileInfo_t *respStructFileInfo);
static CRS_retVal_t convertDiscseqScript(CRS_invLine_t *invStruct,
                                         crs_discseqLine_t *discStruct,
                                         char *newDiscScript);
static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
                                uint32_t numLines, char *respLine);
static void uploadDiscLinesCb(const FPGA_cbArgs_t _cbArgs);
static void uploadPackageCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal);
static CRS_retVal_t insertParam(char *paramValue,
                                crs_fileInfo_t *respStructFileInfo, int index);
static void uploadPackageSingleLineCb(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t findScriptNameIdx(char *scriptName, int *respIdx);
CRS_retVal_t Config_configInit(void *sem)
{
    collectorSem = sem;

}

void Config_process(void)
{
    CRS_retVal_t rspStatus;

    if (gConfigEvents & RUN_NEXT_LINE_EV)
    {
        CRS_LOG(CRS_DEBUG,"in run next line ev\r\n");
        char line[300] = { 0 };
        memset(&gInvLineStrct, 0, sizeof(CRS_invLine_t));
        memset(gDiscExpectVal, 0, EXPECTEDVAL_SZ);

        rspStatus = Config_getInvLine(gInvName, gInvLineNumber,
                                      gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);

            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        rspStatus = Config_parseInvLine(line, &gInvLineStrct);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);

            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        if (gInvLineStrct.DiscSeq == NONE || gInvLineStrct.Package == NONE)
        {
            Semaphore_post(collectorSem);
            gInvLineNumber++;
            return;
        }

        memset(line, 0, 300);
        rspStatus = Config_getDiscoveryLine(gInvLineStrct.DiscSeq,
                                            gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        crs_discseqLine_t discLineStruct;
        rspStatus = Config_parseDiscoveryLine(line, &discLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }
        char newDiscScript[SCRIPT_SZ + 40] = { 0 };

        convertDiscseqScript(&gInvLineStrct, &discLineStruct, newDiscScript);
        memcpy(gDiscExpectVal, discLineStruct.expectedVal, EXPECTEDVAL_SZ);

        Fpga_writeMultiLine(newDiscScript, uploadDiscLinesCb);

        Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    }

    if (gConfigEvents & FINISHED_DISCOVERY_EV)
    {
        CRS_LOG(CRS_DEBUG,"in finished disc ev");

        if (cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) != CRS_SUCCESS)
        {
            CLI_cliPrintf("\r\nDiscovery didnt success");
            gInvLineNumber++;

            Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            Semaphore_post(collectorSem);
            return;
        }

        char line[300] = { 0 };
        rspStatus = Config_getPackageLine(gInvLineStrct.Package,
                                          gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            CLI_cliPrintf("\r\nConfig_getPackageLine didnt success");
            CRS_free(gFileContentCache);

            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        crs_package_t packageLineStruct={0};
        rspStatus = Config_parsePackageLine(line, &packageLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
            CLI_cliPrintf("\r\Config_parsePackageLine didnt success");
            CRS_free(gFileContentCache);
            const FPGA_cbArgs_t cbArgs;
            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }
//        int i = 0;
//        int j = 0;
//        for (i = 0; i < FILEINFO_SZ; ++i)
//        {
//            for (j = 0; j < NAME_VALUES_SZ; ++j)
//            {
//                if (memcmp(packageLineStruct.fileInfos[i].nameValues[j].name,
//                           gFileInfos[i].nameValues[j].name, NAMEVALUE_NAME_SZ)
//                        == 0)
//                {
//                    packageLineStruct.fileInfos[i].nameValues[j].value =
//                            gFileInfos[i].nameValues[j].value;
//                }else{
//                    //TODO: add this param
//                }
//            }
//
//        }

        gInvLineNumber++;
        if (gIsSingleLine == true)
        {
            rspStatus = MultiFiles_runMultiFiles(&packageLineStruct,
                                                 gInvLineStrct.ChipType,
                                                 gInvLineStrct.Flavor,
                                                 uploadPackageSingleLineCb);
        }
        else
        {
            rspStatus = MultiFiles_runMultiFiles(&packageLineStruct,
                                                 gInvLineStrct.ChipType,
                                                 gInvLineStrct.Flavor,
                                                 uploadPackageCb);
        }

        Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
    }
    if (gConfigEvents & FINISHED_SINGLE_LINE_EV)
    {
        CRS_free(gFileContentCache);
        const FPGA_cbArgs_t cbArgs;
        gCbFn(cbArgs);
        Util_clearEvent(&gConfigEvents, FINISHED_SINGLE_LINE_EV);

    }

}

static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal)
{
    if (memcmp(&gDiscExpectVal[2], &gDiscRdRespLine[6],
               strlen(&gDiscExpectVal[2])) != 0)
    {
        if (((!strstr(expVal, "16b'")) && (!strstr(expVal, "32b'"))))
        {
            return CRS_FAILURE;
        }

    }

    if (((!strstr(expVal, "16b'")) && (!strstr(expVal, "32b'"))))
    {
        return CRS_SUCCESS;
    }
    int i = 0;
    char tokenValue[CRS_NVS_LINE_BYTES] = { 0 };
    memcpy(tokenValue, &expVal[4], CRS_NVS_LINE_BYTES - 4);

    if (memcmp(expVal, "16b", 3) == 0)
    {

        for (i = 0; i < 16; i++)
        {
            uint16_t val = CLI_convertStrUint(&rsp[6]);
            uint16_t valPrev = val;
            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val &= ~(1 << (15 - i));
                if (val != (~(0)))
                {
                    return CRS_FAILURE;
                }
            }
            else if (tokenValue[i] == '0')
            {
                val &= (1 << (15 - i));
                if (val != 0)
                {
                    return CRS_FAILURE;
                }
            }

        }

    }
    else if (memcmp(expVal, "32b", 3) == 0)
    {

        for (i = 0; i < 32; i++)
        {
            uint32_t val = CLI_convertStrUint(&rsp[6]);
            uint32_t valPrev = val;

            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val |= (1 << (31 - i));
            }
            else if (tokenValue[i] == '0')
            {
                val &= ~(1 << (31 - i));
            }

            if (val != valPrev)
            {
                return CRS_FAILURE;
            }
        }

//        int2hex(val, valStr);

    }

    return CRS_SUCCESS;

}

static void uploadPackageCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}

static void uploadPackageSingleLineCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gConfigEvents, FINISHED_SINGLE_LINE_EV);
    Semaphore_post(collectorSem);
}

static void uploadDiscLinesCb(const FPGA_cbArgs_t _cbArgs)
{
    char *line = _cbArgs.arg3;
    uint32_t size = _cbArgs.arg0;

    memset(gDiscRdRespLine, 0, TEMP_SZ);

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
            gDiscRdRespLine[gTmpLine_idx] = line[counter];
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
            gDiscRdRespLine[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }
    //    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
    Util_setEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);

    Semaphore_post(collectorSem);
}

static CRS_retVal_t convertDiscseqScript(CRS_invLine_t *invStruct,
                                         crs_discseqLine_t *discStruct,
                                         char *newDiscScript)
{
    char *moveChip = invStruct->Addr;
//    memcpy(newDiscScript, moveChip, strlen(moveChip));
//    newDiscScript[strlen(newDiscScript)] = '\n';
    char discScript[200] = { 0 };

    if (memcmp(invStruct->Addr, "None", strlen("None")) != 0)
    {
        memcpy(newDiscScript, "wr 0xb 0x1\n", strlen("wr 0xb 0x1\n"));
        newDiscScript = newDiscScript + strlen(newDiscScript);
        sprintf(discScript, "%s;%s", moveChip, discStruct->script);
    }
    else
    {
        memcpy(newDiscScript, "wr 0xb 0x1\n", strlen("wr 0xb 0x1\n"));
        newDiscScript = newDiscScript + strlen(newDiscScript);
        sprintf(discScript, "%s", discStruct->script);

    }

    char tmp[50] = { 0 };
    int y = 0;
    while (discScript[y] != 0)
    {
        memset(tmp, 0, 50);
        int x = 0;
        while (discScript[y] != ';' && discScript[y] != 0)
        {
            tmp[x] = discScript[y];
            x++;
//            discScript++;
            y++;
        }

        char lineToSendArray[9][CRS_NVS_LINE_BYTES] = { 0 };
        if (invStruct->ChipType == DIG)
        {
            Convert_readLineDig(tmp, lineToSendArray, invStruct->Flavor);
        }
        else if (invStruct->ChipType == RF)
        {
            Convert_readLineRf(tmp, lineToSendArray, 0, invStruct->Flavor);
        }
        else
        {
            Convert_readLineFpga(tmp, lineToSendArray);
        }

        int i = 0;
        while (lineToSendArray[i][0] != 0)
        {
            i++;
        }
        flat2DArray(lineToSendArray, i, &newDiscScript[strlen(newDiscScript)]);
        newDiscScript[strlen(newDiscScript)] = '\n';

        if (discScript[y] == ';')
        {
//            discScript++;
            y++;
        }
    }
    return CRS_SUCCESS;

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

CRS_retVal_t Config_runConfigDirect(char *filename, char *type, char *fileInfos,
                                    FPGA_cbFn_t cbFunc)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;

    }
    gIsSingleLine = false;
    gInvLineNumber = 0;
    memset(gInvName, 0, 30);
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    memset(&gInvLineStrct, 0, sizeof(CRS_invLine_t));
    if (filename == NULL)
    {
        CLI_cliPrintf("\r\nfilename is null!");
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;
    }
    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
        const FPGA_cbArgs_t cbArgs = { 0 };
        cbFunc(cbArgs);
        return CRS_FAILURE;
    }
    gCbFn = cbFunc;
    memcpy(gInvName, "NAME INV1", strlen("NAME INV1"));
    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    CLI_cliPrintf("\r\n");
    if (memcmp(type, "SN", 2) == 0)
    {
//        crs_package_t packageLineStruct;
//        packageLineStruct.fileInfos[0].name = filename;
//        packageLineStruct.fileInfos[0].type = SC;
//        packageLineStruct.fileInfos[0].nameValues[0].name = filename;
//        packageLineStruct.fileInfos[0].nameValues[0].value = 0;
//        Semaphore_post(collectorSem);
        return CRS_SUCCESS;
    }
}
    CRS_retVal_t Config_runConfigFile(char *filename, FPGA_cbFn_t cbFunc)
    {
        if (Fpga_isOpen() == CRS_FAILURE)
        {
            CLI_cliPrintf("\r\nOpen Fpga first");
            const FPGA_cbArgs_t cbArgs = { 0 };
            cbFunc(cbArgs);
            return CRS_FAILURE;

        }
        gIsSingleLine = false;
        gInvLineNumber = 0;
        memset(gInvName, 0, 30);
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
        memset(&gInvLineStrct, 0, sizeof(CRS_invLine_t));
        if (filename == NULL)
        {
            CLI_cliPrintf("\r\nfilename is null!");
            const FPGA_cbArgs_t cbArgs = { 0 };
            cbFunc(cbArgs);
            return CRS_FAILURE;
        }
        gFileContentCache = Nvs_readFileWithMalloc(filename);
        if (gFileContentCache == NULL)
        {
            const FPGA_cbArgs_t cbArgs = { 0 };
            cbFunc(cbArgs);
            return CRS_FAILURE;
        }
        gCbFn = cbFunc;
        memcpy(gInvName, "NAME INV1", strlen("NAME INV1"));
        Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
        CLI_cliPrintf("\r\n");

        Semaphore_post(collectorSem);
        return CRS_SUCCESS;

    }

//TODO: convert fileInfos to gFileInfos.
    CRS_retVal_t Config_runConfigFileLine(char *filename, uint32_t lineNum,
                                          char *fileInfos, FPGA_cbFn_t cbFunc)
    {

//        if (Fpga_isOpen() == CRS_FAILURE)
//        {
//            CLI_cliPrintf("\r\nOpen Fpga first");
//            const FPGA_cbArgs_t cbArgs = { 0 };
//            cbFunc(cbArgs);
//            return CRS_FAILURE;
//
//        }
//
//        gIsSingleLine = true;
//        gInvLineNumber = lineNum;
//        memset(gInvName, 0, 30);
////    memset(gFileContentCache, 0, FILE_CACHE_SZ);
//        memset(&gInvLineStrct, 0, sizeof(CRS_invLine_t));
//        memset(gFileInfos, 0, sizeof(gFileInfos));
//
//        if (filename == NULL)
//        {
//            CLI_cliPrintf("\r\nfilename is null!");
//            const FPGA_cbArgs_t cbArgs = { 0 };
//            cbFunc(cbArgs);
//            return CRS_FAILURE;
//        }
//
//        gFileContentCache = Nvs_readFileWithMalloc(filename);
//        if (gFileContentCache == NULL)
//        {
//            const FPGA_cbArgs_t cbArgs = { 0 };
//            cbFunc(cbArgs);
//            return CRS_FAILURE;
//        }
//
//        uint32_t numOfParams = 0;
//
//        const char s[2] = " ";
//        char *token;
//        token = strtok(fileInfos, s);
//        int i = 0;
//        char *ptr;
//        char fileNameScript[FILENAME_SZ] = { 0 };
//        int fileNameIdx = 0;
//        while (token != NULL)
//        {
//            ptr = token;
//            while (*ptr != ':')
//            {
//                fileNameScript[i] = *ptr;
//                ptr++;
//                i++;
//            }
//            i = 0;
//            ptr++; //skip ':'
//            findScriptNameIdx(fileNameScript, &fileNameIdx);
//            memcpy(gFileInfos[fileNameIdx].name, fileNameScript, FILENAME_SZ);
//            insertParam(ptr, gFileInfos, fileNameIdx);
//            numOfParams++;
//            i = 0;
//            token = strtok(NULL, s);
//        }
//
//        gCbFn = cbFunc;
//        memcpy(gInvName, "NAME INV1", strlen("NAME INV1"));
//
//        Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
//        CLI_cliPrintf("\r\n");
//
//        Semaphore_post(collectorSem);
//        return CRS_SUCCESS;

    }

    CRS_retVal_t Config_getInvLine(char *invName, uint32_t lineNum,
                                   char *fileContent, char *respLine)
    {

        char *ret = strstr(fileContent, invName);
        if (ret == NULL)
        {
            return CRS_FAILURE;
        }
        char lineNumHex[8] = { 0 };
        sprintf(lineNumHex, "%d,0x", lineNum);
        ret = strstr(ret, lineNumHex);
        if (ret == NULL)
        {
            return CRS_FAILURE;
        }
        while (*ret != '\n')
        {
            *respLine = *ret;
            ret++;
            respLine++;
        }
        *respLine = '\0';
        return CRS_SUCCESS;

    }

    CRS_retVal_t Config_getPackageLine(uint32_t lineNum, char *fileContent,
                                       char *respLine)
    {
        char *ret = strstr(fileContent, "[PACKAGES]");
        char lineNumHex[3] = { 0 };
        sprintf(lineNumHex, "%x,", lineNum);
        ret = strstr(ret, lineNumHex);
        while (*ret != '\n')
        {
            *respLine = *ret;
            ret++;
            respLine++;
        }
        *respLine = '\0';
        return CRS_SUCCESS;

    }

    CRS_retVal_t Config_getDiscoveryLine(uint32_t lineNum, char *fileContent,
                                         char *respLine)
    {
        char *ret = strstr(fileContent, "[DISCSEQ]");
        char lineNumHex[3] = { 0 };
        sprintf(lineNumHex, "%x,", lineNum);
        ret = strstr(ret, lineNumHex);
        while (*ret != '\n')
        {
            *respLine = *ret;
            ret++;
            respLine++;
        }
        *respLine = '\0';
        return CRS_SUCCESS;

    }

    CRS_retVal_t Config_parseInvLine(char *buff,
                                     CRS_invLine_t *respStructInventory)
    {
        // Order,ClassID,Name,Addr,DiscSeq,Mandatory(True|False),ChipType(RF|DIG),Flavor(SPI_SLAVE|SPI_NATIVE),Package(seperated by ; and uploaded to lines in brackets [seperated by |]
        memset(respStructInventory->Addr, 0, ADDR_SZ);
        memset(respStructInventory->ClassID, 0, CLASSID_SZ);
        memset(respStructInventory->Name, 0, NAME_SZ);
        uint32_t ret = 0;
        char temp[TEMP_SZ] = { 0 };
        int i = 0;
        //order
        while (*buff != ',')
        {
            temp[i] = *buff;
            buff++;
            i++;
        }
        ret = strtoul(temp, NULL, 16);
        respStructInventory->order = ret;
        buff++;
        i = 0;
        //classId
        while (*buff != ',')
        {
            respStructInventory->ClassID[i] = *buff;
            buff++;
            i++;
        }
        buff++;
        i = 0;
        //Name
        while (*buff != ',')
        {
            respStructInventory->Name[i] = *buff;
            buff++;
            i++;
        }
        buff++;
        i = 0;
        //Addr
        while (*buff != ',')
        {
            respStructInventory->Addr[i] = *buff;
            buff++;
            i++;
        }
        buff++;
        i = 0;
        //DISCSEQ
        memset(temp, 0, TEMP_SZ);
        while (*buff != ',')
        {
            temp[i] = *buff;
            buff++;
            i++;
        }

        if (strlen(temp) == strlen("None")
                && memcmp(temp, "None", strlen(temp)) == 0)
        {

            respStructInventory->DiscSeq = NONE;
        }
        else
        {
            ret = 0;
            ret = strtoul(temp, NULL, 16);
            respStructInventory->DiscSeq = ret;
        }

        buff++;
        i = 0;
        //MANDTORY
        memset(temp, 0, TEMP_SZ);
        while (*buff != ',')
        {
            temp[i] = *buff;
            buff++;
            i++;
        }

        if (strlen(temp) == strlen("None")
                && memcmp(temp, "None", strlen(temp)) == 0)
        {

            respStructInventory->Mandatory = NONE;
        }
        else
        {
            if (memcmp(temp, "True", sizeof("True")) == 0)
            {
                respStructInventory->Mandatory = true;
            }
            else if (memcmp(temp, "False", sizeof("False")) == 0)
            {
                respStructInventory->Mandatory = false;
            }
        }

        buff++;
        i = 0;
        //CHIPTYPE
        memset(temp, 0, TEMP_SZ);
        while (*buff != ',')
        {
            temp[i] = *buff;
            buff++;
            i++;
        }

        if (strlen(temp) == strlen("None")
                && memcmp(temp, "None", strlen(temp)) == 0)
        {

            respStructInventory->ChipType = UNKNOWN;
        }
        else
        {
            if (memcmp(temp, "RF", sizeof("RF")) == 0)
            {
                respStructInventory->ChipType = RF;
            }
            else if (memcmp(temp, "DIG", sizeof("DIG")) == 0)
            {
                respStructInventory->ChipType = DIG;
            }
        }

        buff++;
        i = 0;
        //FLAVOUR
        memset(temp, 0, TEMP_SZ);
        while (*buff != ',')
        {
            temp[i] = *buff;
            buff++;
            i++;
        }
        if (memcmp(temp, "SPI_NATIVE", sizeof("SPI_NATIVE")) == 0)
        {
            respStructInventory->Flavor = MODE_NATIVE;
        }
        else if (memcmp(temp, "SPI_SLAVE", sizeof("SPI_SLAVE")) == 0)
        {
            respStructInventory->Flavor = MODE_SPI_SLAVE;
        }
        buff++;
        i = 0;
        //PACKAGEID
        memset(temp, 0, TEMP_SZ);
        while (*buff != 0)
        {
            temp[i] = *buff;
            buff++;
            i++;
        }

        ret = 0;
        ret = strtoul(temp, NULL, 16);
        respStructInventory->Package = ret;
        return CRS_SUCCESS;
    }
    CRS_retVal_t Config_parsePackageLine(char *buff,
                                         crs_package_t *respStructPackage)
    {
        char temp[TEMP_SZ] = { 0 };
        uint32_t i = 0;
        uint32_t lineNum = 0;
        while (*buff != 0)
        {
            temp[i] = (*buff);
            i++;
            buff++;
            if (*buff == ',')
            {
                respStructPackage->index = CLI_convertStrUint(temp);
                buff++;
                memset(temp, 0, TEMP_SZ);
                i = 0;
            }
            else if (*buff == 0)
            {
                parseFileInfo(temp, respStructPackage->fileInfos);
                int cntrChar = 1;
                int j = 0;
                while (temp[j] != 0)
                {
                    if (temp[j] == ';')
                    {
                        cntrChar++;
                    }
                    j++;
                }
                respStructPackage->numFileInfos = cntrChar;
            }

        }
        return CRS_SUCCESS;

    }
    CRS_retVal_t Config_parseDiscoveryLine(char *buff,
                                           crs_discseqLine_t *respStructDiscseq)
    {
        char temp[TEMP_SZ] = { 0 };
        uint32_t i = 0;
        while (*buff != ',')
        {
            temp[i] = (*buff);
            i++;
            buff++;
            if (*buff == ',')
            {
                respStructDiscseq->index = CLI_convertStrUint(temp);

            }
        }
        buff++;
        memset(respStructDiscseq->script, 0, SCRIPT_SZ);
        memset(respStructDiscseq->expectedVal, 0, EXPECTEDVAL_SZ);
        char *ptr = buff;
        int k = 0;
        while (*ptr != ':')
        {
            respStructDiscseq->script[k] = *ptr;
            ptr++;
            k++;
        }
        k = 0;
        ptr++;
        while (*ptr)
        {
            respStructDiscseq->expectedVal[k] = *ptr;
            ptr++;
            k++;
        }
        return CRS_SUCCESS;

    }

    static CRS_retVal_t parseFileInfo(char *buff,
                                      crs_fileInfo_t *respStructFileInfo)
    {

        char temp[TEMP_SZ] = { 0 };
        uint32_t i = 0;
        uint32_t index = 0;
        for (i = 0; i < NAME_VALUES_SZ; ++i)
        {
            memset(respStructFileInfo->nameValues[i].name, 0,
                   NAMEVALUE_NAME_SZ);
        }
        i = 0;
        while (*buff != NULL)
        {
            temp[i] = (*buff);
            i++;
            buff++;
            if (*buff == '[')
            {
                memset(respStructFileInfo[index].name, 0, FILENAME_SZ);
                memcpy(respStructFileInfo[index].name, temp, strlen(temp));
                i = 0;
                memset(temp, 0, TEMP_SZ);
                while (*buff != '[')
                {
                    buff++;
                }
                buff++;
                while (*buff != ']')
                {
                    temp[i] = (*buff);
                    i++;
                    buff++;
                }
                if (*temp)
                {
                    memset(respStructFileInfo[index].LUTline, 0, LUT_SZ);
                    char *ptr = temp;
                    int k = 0;
                    while (*ptr)
                    {
                        if (*ptr != '|')
                        {
                            respStructFileInfo[index].LUTline[(temp[k]) - '0'] =
                                    temp[k];
                        }
                        ptr++;
                        k++;
                    }
//                memcpy(respStructFileInfo[index].LUTline, temp, strlen(temp));
                    i = 0;
                    memset(temp, 0, TEMP_SZ);
                }
                buff++;
                while (*buff != ']')
                {
                    buff++;
                    if (*buff != ']')
                    {
                        temp[i] = (*buff);
                        i++;
                    }
                }
                if (memcmp(temp, "SN", 2) == 0)
                {
                    respStructFileInfo[index].type = SN;
                }
                else if (memcmp(temp, "SC", 2) == 0)
                {
                    respStructFileInfo[index].type = SC;
                }
                i = 0;
                buff++;
                memset(temp, 0, TEMP_SZ);
                while (*buff != ']')
                {
                    buff++;
                    if (*buff == ',' || *buff == ']')
                    {
                        if (*temp)
                        {
                            insertParam(temp, respStructFileInfo, index);
                            i = 0;
                            memset(temp, 0, TEMP_SZ);
                        }
                    }
                    if (*buff != ']')
                    {
                        temp[i] = (*buff);
                        i++;
                    }
                }
                memset(temp, 0, TEMP_SZ);

            }
            else if (*buff == ';')
            {
                index++;
                buff++;
                i = 0;
                memset(temp, 0, TEMP_SZ);
            }
        }
        return CRS_SUCCESS;

    }
    static CRS_retVal_t insertParam(char *paramValue,
                                    crs_fileInfo_t *respStructFileInfo,
                                    int index)
    {
        char param[NAMEVALUE_NAME_SZ] = { 0 };
        char value[NAMEVALUE_NAME_SZ] = { 0 };
        int32_t valueInt = 0;
        int i = 0;
        char *ptr = paramValue;
        while (*ptr != '=')
        {
            param[i] = *ptr;
            ptr++;
            i++;
        }
        ptr++; //skip ''='
        i = 0;
        while (*ptr)
        {
            value[i] = *ptr;
            ptr++;
            i++;
        }
        valueInt = strtol(value, NULL, 10);
        int idx = 0;
        while (1)
        {
            if (*(respStructFileInfo[index].nameValues[idx].name) == 0)
            {
                memcpy(respStructFileInfo[index].nameValues[idx].name, param,
                NAMEVALUE_NAME_SZ);
                respStructFileInfo[index].nameValues[idx].value = valueInt;
                break;
            }
            idx++;
        }
    }

    static CRS_retVal_t findScriptNameIdx(char *scriptName, int *respIdx)
    {
//        int i = 0;
//        while (i < NAME_VALUES_SZ)
//        {
//            if (memcmp(scriptName, gFileInfos, FILENAME_SZ) == 0)
//            {
//                *respIdx = i;
//                return CRS_SUCCESS;
//            }
//            i++;
//        }
//        *respIdx = 0;
//        return CRS_SUCCESS;
    }

