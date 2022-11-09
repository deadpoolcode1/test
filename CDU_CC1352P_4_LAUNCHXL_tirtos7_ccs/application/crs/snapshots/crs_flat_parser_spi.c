/*
 * crs_flat_parser_spi.c
 *
 *  Created on: Nov 3, 2022
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
//#include "config_parsing.h"
#include <ti/sysbios/knl/Semaphore.h>
#include "application/crs/snapshots/crs_flat_parser_spi.h"
//#include "application/crs/crs_fpga.h" //TODO: replace
#include "application/crs/crs_tmp.h"
#include "crs_convert_snapshot.h"
#include "crs_multi_snapshots_spi.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define TEMP_SZ 200
#define NAME_SIZE   30
#define DISCOVERY_ARR_SIZE  30
#define DISCSEQ_SZ 4
#define PACKGES_SZ 5
#define FILE_CACHE_SZ 100
#define NONE 255
#define INV_NAME    "NAME INV1"

#define RUN_NEXT_LINE_EV 0x1
#define FINISHED_DISCOVERY_EV 0x2
#define FINISHED_SINGLE_LINE_EV 0x4

typedef struct fileTraverser
{
    SPI_CRS_invLine_t invLineStruct;
    char discRdRespLine[TEMP_SZ];
    char invName[NAME_SIZE];
    char discExpectVal[EXPECTEDVAL_SZ];
    char *fileContentCache;
    uint32_t invLineNumber;
    bool isSingleLine;
    bool isOnlyDiscovery;
    uint32_t discoveryArr[DISCOVERY_ARR_SIZE];
    uint32_t discoveryArrIdx;
    bool isConfigStatusFail;
}flatFileTraverser_t;

/******************************************************************************
 Local variables
 *****************************************************************************/
static SPI_CRS_invLine_t gInvLineStrct = { 0 };
static char gDiscRdRespLine[TEMP_SZ] = { 0 };
static char gInvName[30] = { 0 };
static char gDiscExpectVal[EXPECTEDVAL_SZ] = { 0 };
static Semaphore_Handle collectorSem;
static uint16_t gConfigEvents = 0;
static char *gFileContentCache;
//static uint32_t gFileContentCacheIdx = 0;
static uint32_t gInvLineNumber = 0;
static bool gIsSingleLine = false;
//static FPGA_cbFn_t gCbFn = NULL;
static bool gIsOnlyDiscovery = false;
static uint32_t gDiscoveryArr[30] = {0};
static uint32_t gDiscoveryArrIdx = 0;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t parseFileInfo(char *buff,
                                  SPI_crs_fileInfo_t *respStructFileInfo);
static CRS_retVal_t convertDiscseqScript(SPI_CRS_invLine_t *invStruct,
                                         SPI_crs_discseqLine_t *discStruct,
                                         char *newDiscScript);
static CRS_retVal_t flat2DArray(char lines[9][CRS_NVS_LINE_BYTES],
                                uint32_t numLines, char *respLine);
//static void uploadDiscLinesCb(const FPGA_cbArgs_t _cbArgs); //TODO: replace
static void uploadDiscLines(flatFileTraverser_t *fileTraverser, uint32_t rsp);

//static void uploadPackageCb(const FPGA_cbArgs_t _cbArgs); //TODO: replace
static void uploadPackageCb(void);
//static void uploadPackageSingleLineCb(const FPGA_cbArgs_t _cbArgs); //TODO: replace
static void uploadPackageSingleLineCb(void);
static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal);
static CRS_retVal_t insertParam(char *paramValue,
                                SPI_crs_fileInfo_t *respStructFileInfo, int index);

static CRS_retVal_t isValInArr(uint32_t *arr, uint32_t arrLen, uint32_t expVal);

static CRS_retVal_t runFile(flatFileTraverser_t *fileTraverser);
CRS_retVal_t finishedDiscovery(flatFileTraverser_t *fileTraverser);


/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t SPI_Config_configInit(void *sem)
{
    collectorSem = sem;
return CRS_SUCCESS;
}

CRS_retVal_t SPI_Config_runConfigDirect(char *filename, char *type, char *fileInfos)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        return CRS_FAILURE;

    }
    gIsOnlyDiscovery = false;

    gIsSingleLine = false;
    gInvLineNumber = 0;
    memset(gInvName, 0, 30);
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    memset(&gInvLineStrct, 0, sizeof(SPI_CRS_invLine_t));
    if (filename == NULL)
    {
        CLI_cliPrintf("\r\nfilename is null!");
//        const FPGA_cbArgs_t cbArgs = { 0 }; //TODO: replace
//        cbFunc(cbArgs); //TODO: replace
        return CRS_FAILURE;
    }
    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
//        const FPGA_cbArgs_t cbArgs = { 0 }; //TODO: replace
//        cbFunc(cbArgs); //TODO: replace
        return CRS_FAILURE;
    }
//    gCbFn = cbFunc;
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
    return CRS_SUCCESS;
}

CRS_retVal_t SPI_Config_runConfigFile(char *filename)
{
//    if (Fpga_isOpen() == CRS_FAILURE)
//    {
//        CLI_cliPrintf("\r\nOpen Fpga first");
////        const FPGA_cbArgs_t cbArgs = { 0 };
////        cbFunc(cbArgs);
//        return CRS_FAILURE;
//
//    }
    flatFileTraverser_t fileTraverser = {0};
//    gIsOnlyDiscovery = false;
//    gIsSingleLine = false;
//    gInvLineNumber = 0;
//    memset(gInvName, 0, 30);
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
//    memset(&gInvLineStrct, 0, sizeof(SPI_CRS_invLine_t));
    if (filename == NULL)
    {
        CLI_cliPrintf("\r\nfilename is null!");
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        return CRS_FAILURE;
    }
    fileTraverser.fileContentCache = Nvs_readFileWithMalloc(filename);
//    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (fileTraverser.fileContentCache == NULL)
    {
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        CLI_cliPrintf("\r\nfilename not found!");
        return CRS_FAILURE;
    }
//    gCbFn = cbFunc;
//    memcpy(gInvName, "NAME INV1", strlen("NAME INV1"));
    memcpy(fileTraverser.invName,INV_NAME, strlen(INV_NAME));
//    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    fileTraverser.isConfigStatusFail = false;
    CRS_retVal_t rspStatus = runFile(&fileTraverser);
    CRS_free(fileTraverser.fileContentCache);
    fileTraverser.fileContentCache = NULL;
    if (rspStatus != CRS_SUCCESS || fileTraverser.isConfigStatusFail == true)
    {
        CLI_cliPrintf("\r\nConfig Status: FAIL");
        return CRS_FAILURE;
    }
    else
    {
        CLI_cliPrintf("\r\nConfig Status: OK");
    }
    CLI_cliPrintf("\r\n");

    return CRS_SUCCESS;

}

CRS_retVal_t SPI_Config_runConfigFileDiscovery(char *filename)
{
    if (Fpga_isOpen() == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nOpen Fpga first");
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        return CRS_FAILURE;

    }
    memset(gDiscoveryArr, 0, sizeof(gDiscoveryArr));
    gDiscoveryArrIdx = 0;
    gIsOnlyDiscovery = true;

    gIsSingleLine = false;
    gInvLineNumber = 0;
    memset(gInvName, 0, 30);
//    memset(gFileContentCache, 0, FILE_CACHE_SZ);
    memset(&gInvLineStrct, 0, sizeof(SPI_CRS_invLine_t));
    if (filename == NULL)
    {
        CLI_cliPrintf("\r\nfilename is null!");
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        return CRS_FAILURE;
    }
    gFileContentCache = Nvs_readFileWithMalloc(filename);
    if (gFileContentCache == NULL)
    {
//        const FPGA_cbArgs_t cbArgs = { 0 };
//        cbFunc(cbArgs);
        CLI_cliPrintf("\r\nfilename not found!");

        return CRS_FAILURE;
    }
//    gCbFn = cbFunc;
    memcpy(gInvName, "NAME INV1", strlen("NAME INV1"));
    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    //CLI_cliPrintf("\r\n");

    Semaphore_post(collectorSem);
    return CRS_SUCCESS;

}


//TODO: convert fileInfos to gFileInfos.
CRS_retVal_t SPI_Config_runConfigFileLine(char *filename, uint32_t lineNum,
                                      char *fileInfos)
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
//    gIsOnlyDiscovery = false;

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
return CRS_SUCCESS;
}

CRS_retVal_t SPI_Config_getInvLine(char *invName, uint32_t lineNum,
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

CRS_retVal_t SPI_Config_getPackageLine(uint32_t lineNum, char *fileContent,
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

CRS_retVal_t SPI_Config_getDiscoveryLine(uint32_t lineNum, char *fileContent,
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

CRS_retVal_t SPI_Config_parseInvLine(char *buff, SPI_CRS_invLine_t *respStructInventory)
{
    // Order,ClassID,Name,Addr,DiscSeq,Mandatory(True|False),ChipType(RF|DIG),Flavor(SPI_SLAVE|SPI_NATIVE),Package(seperated by ; and uploaded to lines in brackets [seperated by |]
    memset(respStructInventory->Addr, 0, ADDR_SZ);
    memset(respStructInventory->ClassID, 0, CLASSID_SZ);
    memset(respStructInventory->Name, 0, MODULE_NAME_SZ);
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
    ret = strtoul(temp, NULL, 10);
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
CRS_retVal_t SPI_Config_parsePackageLine(char *buff,
                                         SPI_crs_package_t *respStructPackage)
{
    char temp[TEMP_SZ] = { 0 };
    uint32_t i = 0;
//    uint32_t lineNum = 0;
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
CRS_retVal_t SPI_Config_parseDiscoveryLine(char *buff,
                                       SPI_crs_discseqLine_t *respStructDiscseq)
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

void SPI_Config_process(void)
{
    CRS_retVal_t rspStatus;

    if (gConfigEvents & RUN_NEXT_LINE_EV)
    {
        CRS_LOG(CRS_DEBUG, "in run next line ev\r\n");
        char line[300] = { 0 };
        memset(&gInvLineStrct, 0, sizeof(SPI_CRS_invLine_t));
        memset(gDiscExpectVal, 0, EXPECTEDVAL_SZ);

        rspStatus = SPI_Config_getInvLine(gInvName, gInvLineNumber,
                                      gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            if (gIsOnlyDiscovery == true)
            {

            }
            CRS_free(gFileContentCache);

//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            CLI_startREAD();
            return;
        }


        rspStatus = SPI_Config_parseInvLine(line, &gInvLineStrct);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);

//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        if (gInvLineStrct.DiscSeq == NONE || gInvLineStrct.Package == NONE)
        {
            Semaphore_post(collectorSem);
            gInvLineNumber++;
            return;
        }
        //discovery

        uint32_t classId = strtoul(gInvLineStrct.ClassID, NULL, 0);
        if (gIsOnlyDiscovery == true && (isValInArr(gDiscoveryArr, gDiscoveryArrIdx, classId) == CRS_SUCCESS))
        {
            Semaphore_post(collectorSem);
            gInvLineNumber++;
            return;
        }

        if (gIsOnlyDiscovery == true)
        {
            gDiscoveryArr[gDiscoveryArrIdx] = classId;
            gDiscoveryArrIdx++;
        }


        memset(line, 0, 300);
        rspStatus = SPI_Config_getDiscoveryLine(gInvLineStrct.DiscSeq,
                                            gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }

        SPI_crs_discseqLine_t discLineStruct;
        rspStatus = SPI_Config_parseDiscoveryLine(line, &discLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
            CRS_free(gFileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return;
        }
        char newDiscScript[SCRIPT_SZ + 40] = { 0 };

        convertDiscseqScript(&gInvLineStrct, &discLineStruct, newDiscScript);
        memcpy(gDiscExpectVal, discLineStruct.expectedVal, EXPECTEDVAL_SZ);
//        if (gIsOnlyDiscovery == true)
//        {
//            Fpga_writeMultiLineNoPrint(newDiscScript, uploadDiscLinesCb); //TODO: replace
        uint32_t rsp = 0;
        Fpga_tmpWriteMultiLine(newDiscScript, &rsp);
//        CLI_cliPrintf("\r\nsending command %s", newDiscScript);
//        CLI_cliPrintf("\r\n recived %x", rsp);

//        uploadDiscLines(rsp); // todo this is good
//        }
//        else
//        {
//            Fpga_writeMultiLine(newDiscScript, uploadDiscLinesCb);//TODO: replaceQ
//
//        }

        Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    }

    if (gConfigEvents & FINISHED_DISCOVERY_EV)
    {
        CRS_LOG(CRS_DEBUG, "in finished disc ev");

        if (cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) != CRS_SUCCESS)
        {
            if (gIsOnlyDiscovery == true)
            {
                char lineInv[300] = { 0 };

                rspStatus = SPI_Config_getInvLine(gInvName, gInvLineNumber,
                                              gFileContentCache, lineInv);
                if (rspStatus != CRS_SUCCESS)
                {

                    CRS_free(gFileContentCache);

//                    const FPGA_cbArgs_t cbArgs={0};
//                    gCbFn(cbArgs);
                    Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
                    return;
                }
                CLI_cliPrintf("\r\n%s,FAIL", lineInv);


            }
            else
            {
                CLI_cliPrintf("\r\nDiscovery didnt success");

            }
            gInvLineNumber++;

            Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            Semaphore_post(collectorSem);
            return;
        }


        if (gIsOnlyDiscovery == true)
        {
            char lineInv[300] = { 0 };

            rspStatus = SPI_Config_getInvLine(gInvName, gInvLineNumber,
                                          gFileContentCache, lineInv);
            if (rspStatus != CRS_SUCCESS)
            {

                CRS_free(gFileContentCache);

//                const FPGA_cbArgs_t cbArgs={0};
//                gCbFn(cbArgs);
                Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
                return;
            }
            CLI_cliPrintf("\r\n%s,PASS", lineInv);

            gInvLineNumber++;
            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            Semaphore_post(collectorSem);

        }
        CLI_cliPrintf("\r\nDiscovery success");

        char line[300] = { 0 };
        rspStatus = SPI_Config_getPackageLine(gInvLineStrct.Package,
                                          gFileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            CLI_cliPrintf("\r\nConfig_getPackageLine didnt success");
            CRS_free(gFileContentCache);
//
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            return;
        }

        SPI_crs_package_t packageLineStruct = { 0 };
        rspStatus = SPI_Config_parsePackageLine(line, &packageLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
            CLI_cliPrintf("\r\nConfig_parsePackageLine didnt succeed");
            CRS_free(gFileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            return;
        }

        gInvLineNumber++;
        if (gIsSingleLine == true)
        {
            rspStatus = MultiFilesSPI_runMultiFiles(&packageLineStruct,
                                                 gInvLineStrct.ChipType,
                                                 gInvLineStrct.Flavor);
            uploadPackageSingleLineCb();
        }
        else
        {
            rspStatus = MultiFilesSPI_runMultiFiles(&packageLineStruct,
                                                 gInvLineStrct.ChipType,
                                                 gInvLineStrct.Flavor);
            uploadPackageCb();
        }

        Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
    }
    if (gConfigEvents & FINISHED_SINGLE_LINE_EV)
    {
        CRS_free(gFileContentCache);
//        const FPGA_cbArgs_t cbArgs={0};
//        gCbFn(cbArgs);
        Util_clearEvent(&gConfigEvents, FINISHED_SINGLE_LINE_EV);

    }

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t isValInArr(uint32_t *arr, uint32_t arrLen, uint32_t expVal)
{
    int i = 0;
    for (i = 0; i < arrLen; i++)
    {
        if (arr[i] == expVal)
        {
            return CRS_SUCCESS;
        }
    }
    return CRS_FAILURE;

}


static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal)
{
//    if (memcmp(&gDiscExpectVal[2], &gDiscRdRespLine[6],
//               strlen(&gDiscExpectVal[2])) != 0)
    uint32_t offset = strlen(rsp) - 4; // compare last 4 bytes
    if (memcmp(&expVal[2], &rsp[offset],
               strlen(&expVal[2])) != 0)
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
//            uint16_t valPrev = val;
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

//static void uploadPackageCb(const FPGA_cbArgs_t _cbArgs) //TODO: replace
//{
//    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
//    Semaphore_post(collectorSem);
//}

static void uploadPackageCb(void)
{
    Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}
//static void uploadPackageSingleLineCb(const FPGA_cbArgs_t _cbArgs)
//{
//    Util_setEvent(&gConfigEvents, FINISHED_SINGLE_LINE_EV);
//    Semaphore_post(collectorSem);
//}

static void uploadPackageSingleLineCb(void)
{
    Util_setEvent(&gConfigEvents, FINISHED_SINGLE_LINE_EV);
    Semaphore_post(collectorSem);
}

//static void uploadDiscLinesCb(const FPGA_cbArgs_t _cbArgs) //TODO: replace
//{
//    char *line = _cbArgs.arg3; // TODO get around this
////    uint32_t size = _cbArgs.arg0;
//
//    memset(gDiscRdRespLine, 0, TEMP_SZ);
//
//    int gTmpLine_idx = 0;
//    int counter = 0;
//    bool isNumber = false;
//    bool isFirst = true;
//    while (memcmp(&line[counter], "AP>", 3) != 0)
//    {
//        if (line[counter] == '0' && line[counter + 1] == 'x')
//        {
//            if (isFirst == true)
//            {
//                counter++;
//                isFirst = false;
//                continue;
//
//            }
//            isNumber = true;
//            gDiscRdRespLine[gTmpLine_idx] = line[counter];
//            gTmpLine_idx++;
//            counter++;
//            continue;
//        }
//
//        if (line[counter] == '\r' || line[counter] == '\n')
//        {
//            isNumber = false;
//        }
//
//        if (isNumber == true)
//        {
//            gDiscRdRespLine[gTmpLine_idx] = line[counter];
//            gTmpLine_idx++;
//            counter++;
//            continue;
//
//        }
//        counter++;
//    }
//    //    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
//    Util_setEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
//
//    Semaphore_post(collectorSem);
//}

static CRS_retVal_t convertDiscseqScript(SPI_CRS_invLine_t *invStruct,
                                         SPI_crs_discseqLine_t *discStruct,
                                         char *newDiscScript)
{
    char *moveChip = invStruct->Addr;
//    memcpy(newDiscScript, moveChip, strlen(moveChip));
//    newDiscScript[strlen(newDiscScript)] = '\n';
    char discScript[200] = { 0 };

    if (memcmp(invStruct->Addr, "None", strlen("None")) != 0)
    {
        memcpy(newDiscScript, "wr 0xb 0x1\r\n", strlen("wr 0xb 0x1\r\n"));
        newDiscScript = newDiscScript + strlen(newDiscScript);
        sprintf(discScript, "%s;%s", moveChip, discStruct->script);
    }
    else
    {
        memcpy(newDiscScript, "wr 0xb 0x1\r\n", strlen("wr 0xb 0x1\r\n"));
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
        uint32_t len = strlen(respLine);
        respLine[len] = '\r';
        respLine[len + 1] = '\n';
        strcat(respLine, lines[i]);
    }
    return CRS_SUCCESS;

}

static CRS_retVal_t parseFileInfo(char *buff,
                                  SPI_crs_fileInfo_t *respStructFileInfo)
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
                respStructFileInfo[index].type = SPI_SN;
            }
            else if (memcmp(temp, "SC", 2) == 0)
            {
                respStructFileInfo[index].type = SPI_SC;
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
                                SPI_crs_fileInfo_t *respStructFileInfo, int index)
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
    return CRS_SUCCESS;
}

static void uploadDiscLines(flatFileTraverser_t *fileTraverser, uint32_t rsp)
{
    memset(fileTraverser->discRdRespLine, 0, TEMP_SZ);

//   int tmpLine_idx = 0;
//   int counter = 0;
//   bool isNumber = false;
//   bool isFirst = true;
//   while (memcmp(&line[counter], "AP>", 3) != 0)
//   {
//       if (line[counter] == '0' && line[counter + 1] == 'x')
//       {
//           if (isFirst == true)
//           {
//               counter++;
//               isFirst = false;
//               continue;
//
//           }
//           isNumber = true;
//           gDiscRdRespLine[tmpLine_idx] = line[counter];
//           tmpLine_idx++;
//           counter++;
//           continue;
//       }
//
//       if (line[counter] == '\r' || line[counter] == '\n')
//       {
//           isNumber = false;
//       }
//
//       if (isNumber == true)
//       {
//           gDiscRdRespLine[tmpLine_idx] = line[counter];
//           tmpLine_idx++;
//           counter++;
//           continue;
//
//       }
//       counter++;
//   }

   //    CLI_cliPrintf("\r\nrd rsp after my parsing: %s\r\n", gTmpLine);
    fileTraverser->discRdRespLine[0] = '0';
    fileTraverser->discRdRespLine[1] = 'x';

   sprintf(fileTraverser->discRdRespLine+2, "%x", rsp);
//   Util_setEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);

}

static CRS_retVal_t runFile(flatFileTraverser_t *fileTraverser)
{
    CRS_retVal_t rspStatus = CRS_SUCCESS;
    while(true)
    {
        char line[300] = { 0 };
        memset(&fileTraverser->invLineStruct, 0, sizeof(SPI_CRS_invLine_t));
        memset(fileTraverser->discExpectVal, 0, EXPECTEDVAL_SZ);

        rspStatus = SPI_Config_getInvLine(fileTraverser->invName,fileTraverser->invLineNumber,
                                                      fileTraverser->fileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
            if (fileTraverser->isOnlyDiscovery == true)
            {

            }
//            CRS_free(fileTraverser->fileContentCache);

            return CRS_SUCCESS;
        }


        rspStatus = SPI_Config_parseInvLine(line,&fileTraverser->invLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
//            CRS_free(fileTraverser->fileContentCache);

            return CRS_FAILURE;
        }

        if (fileTraverser->invLineStruct.DiscSeq == NONE || fileTraverser->invLineStruct.Package == NONE)
        {
            fileTraverser->invLineNumber++;
            continue;
        }
        //discovery

        uint32_t classId = strtoul(fileTraverser->invLineStruct.ClassID, NULL, 0);
        if (fileTraverser->isOnlyDiscovery == true && (isValInArr(fileTraverser->discoveryArr, fileTraverser->discoveryArrIdx, classId) == CRS_SUCCESS))
        {
            fileTraverser->invLineNumber++;
            continue;
        }

        if (fileTraverser->isOnlyDiscovery == true)
        {
            fileTraverser->discoveryArr[fileTraverser->discoveryArrIdx] = classId;
            fileTraverser->discoveryArrIdx++;
        }


        memset(line, 0, 300);
        rspStatus = SPI_Config_getDiscoveryLine(fileTraverser->invLineStruct.DiscSeq,
                                            fileTraverser->fileContentCache, line);
        if (rspStatus != CRS_SUCCESS)
        {
//            CRS_free(fileTraverser->fileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
//                Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return CRS_FAILURE;
        }

        SPI_crs_discseqLine_t discLineStruct = {0};
        rspStatus = SPI_Config_parseDiscoveryLine(line, &discLineStruct);
        if (rspStatus != CRS_SUCCESS)
        {
//            CRS_free(fileTraverser->fileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
//                Util_clearEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
            return CRS_FAILURE;
        }
        char newDiscScript[SCRIPT_SZ + 40] = { 0 };

        convertDiscseqScript(&fileTraverser->invLineStruct, &discLineStruct, newDiscScript);
        memcpy(fileTraverser->discExpectVal, discLineStruct.expectedVal, EXPECTEDVAL_SZ);
//        if (gIsOnlyDiscovery == true)
//        {
//            Fpga_writeMultiLineNoPrint(newDiscScript, uploadDiscLinesCb); //TODO: replace
        uint32_t rsp = 0;
        Fpga_tmpWriteMultiLine(newDiscScript, &rsp);
//        CLI_cliPrintf("\r\nsending command %s", newDiscScript);
//        CLI_cliPrintf("\r\n recived %x", rsp);

        uploadDiscLines(fileTraverser, rsp);
        if (finishedDiscovery(fileTraverser) != CRS_SUCCESS)
        {
            return CRS_FAILURE;
        }
        CLI_cliPrintf("%s",fileTraverser->invLineStruct.LineMessage);
    }
}
    //        }
    //        else
    //        {
    //            Fpga_writeMultiLine(newDiscScript, uploadDiscLinesCb);//TODO: replaceQ
    //
    //        }

CRS_retVal_t finishedDiscovery(flatFileTraverser_t *fileTraverser)
{
    CRS_retVal_t rspStatus = CRS_SUCCESS;
    if (cmpDiscRsp(fileTraverser->discRdRespLine,fileTraverser->discExpectVal) != CRS_SUCCESS)
    {
        if (fileTraverser->isOnlyDiscovery == true)
        {
            char lineInv[300] = { 0 };

            rspStatus = SPI_Config_getInvLine(fileTraverser->invName,fileTraverser->invLineNumber,
                                                  fileTraverser->fileContentCache, lineInv);
            if (rspStatus != CRS_SUCCESS)
            {

//                CRS_free(fileTraverser->fileContentCache);

//                    const FPGA_cbArgs_t cbArgs={0};
//                    gCbFn(cbArgs);
//                        Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
                return CRS_FAILURE;
            }
            CLI_cliPrintf("\r\n%s,FAIL", lineInv);


        }
        else
        {
            sprintf(fileTraverser->invLineStruct.LineMessage, "\r\nLine %d, Module %s: Comm failed",fileTraverser->invLineStruct.order, fileTraverser->invLineStruct.Name);
            fileTraverser->isConfigStatusFail = true;
        }
        fileTraverser->invLineNumber++;

//        Util_setEvent(&gConfigEvents, RUN_NEXT_LINE_EV);
        return CRS_SUCCESS;
    }


    if (fileTraverser->isOnlyDiscovery == true)
    {
        char lineInv[300] = { 0 };

        rspStatus = SPI_Config_getInvLine(fileTraverser->invName,fileTraverser->invLineNumber,
                                      fileTraverser->fileContentCache, lineInv);
        if (rspStatus != CRS_SUCCESS)
        {

//            CRS_free(fileTraverser->fileContentCache);

//                const FPGA_cbArgs_t cbArgs={0};
//                gCbFn(cbArgs);
//            Util_clearEvent(&gConfigEvents, FINISHED_DISCOVERY_EV);
            return CRS_FAILURE;
        }
        CLI_cliPrintf("\r\n%s,PASS", lineInv);

        fileTraverser->invLineNumber++;
    }
    sprintf(fileTraverser->invLineStruct.LineMessage, "\r\nLine %d, Module %s: Comm OK",fileTraverser->invLineStruct.order, fileTraverser->invLineStruct.Name);

    char line[300] = { 0 };
    rspStatus = SPI_Config_getPackageLine(fileTraverser->invLineStruct.Package,
                                      fileTraverser->fileContentCache, line);
    if (rspStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nConfig_getPackageLine didnt success");
//        CRS_free(fileTraverser->fileContentCache);

        return CRS_FAILURE;
    }

    SPI_crs_package_t packageLineStruct = { 0 };
    rspStatus = SPI_Config_parsePackageLine(line, &packageLineStruct);
    if (rspStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nConfig_parsePackageLine didnt succeed");
//        CRS_free(fileTraverser->fileContentCache);
//            const FPGA_cbArgs_t cbArgs={0};
//            gCbFn(cbArgs);
        return CRS_FAILURE;
    }

    fileTraverser->invLineNumber++;
    if (fileTraverser->isSingleLine == true)
    {
        rspStatus = MultiFilesSPI_runMultiFiles(&packageLineStruct,
                                             fileTraverser->invLineStruct.ChipType,
                                             fileTraverser->invLineStruct.Flavor);

//        CRS_free(fileTraverser->fileContentCache);
    }
    else
    {
        rspStatus = MultiFilesSPI_runMultiFiles(&packageLineStruct,
                                                fileTraverser->invLineStruct.ChipType,
                                                fileTraverser->invLineStruct.Flavor);
    }
    if (rspStatus != CRS_SUCCESS)
    {
        strcat(fileTraverser->invLineStruct.LineMessage,
                ", Config Failed");
        return CRS_FAILURE;
    }

    strcat(fileTraverser->invLineStruct.LineMessage,
            ", Config OK");
    return CRS_SUCCESS;
}

