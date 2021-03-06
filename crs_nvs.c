/*
 * crs_nvs.c
 *
 *  Created on: 20 ????? 2021
 *      Author: cellium
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_nvs.h"
#include <ti/sysbios/knl/Task.h>

/******************************************************************************
 Constants and definitions
 *****************************************************************************/


/******************************************************************************
 Global variables
 *****************************************************************************/
char *bufCache = NULL;

/******************************************************************************
 Local variables
 *****************************************************************************/
static NVS_Handle gNvsHandle;
static NVS_Attrs gRegionAttrs;
//static Semaphore_Handle collectorSem;
static uint8_t gFAT_sector_sz;
static uint32_t gNumFiles;
static bool gIsMoudleInit = false;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
//static void printStatus(int_fast16_t retStatus);
static CRS_retVal_t Nvs_readFAT(CRS_FAT_t *fat);
static CRS_retVal_t Nvs_writeFAT(CRS_FAT_t *fat);
static CRS_retVal_t Nvs_readFATNumFiles(CRS_FAT_t *fat, uint32_t start,
                                        uint32_t numFiles);

static CRS_retVal_t nvsInit();
static CRS_retVal_t nvsClose();

static CRS_FAT_t FATcache[FAT_CACHE_SZ + 1] = { 0 };

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t Nvs_init(void *sem)
{
//    collectorSem = sem;
//    NVS_Params nvsParams;
    NVS_init();
//    NVS_Params_init(&nvsParams);
//    gNvsHandle = NVS_open(FS_NVS_0, &nvsParams);
//
//    if (gNvsHandle == NULL)
//    {
//        CLI_cliPrintf("NVS_open() failed.\r\n");
//
//        return (CRS_FAILURE);
//    }
//    /*
//     * This will populate a NVS_Attrs structure with properties specific
//     * to a NVS_Handle such as region base address, region size,
//     * and sector size.
//     */
//    NVS_getAttrs(gNvsHandle, &gRegionAttrs);
//    uint32_t numFiles = (gRegionAttrs.regionSize / gRegionAttrs.sectorSize);
//    gFAT_sector_sz = (numFiles * sizeof(CRS_FAT_t)
//            + (gRegionAttrs.sectorSize - 1)) / gRegionAttrs.sectorSize;
//    gFAT_sector_sz++;
//    gNumFiles = numFiles - gFAT_sector_sz;
//    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);

    return CRS_SUCCESS;

}

CRS_retVal_t Nvs_ls(uint8_t page)
{

    /* Display the NVS region attributes. */
//    CLI_cliPrintf("\r\nMaxFileSize=0x%x", gRegionAttrs.sectorSize);
//    CLI_cliPrintf("\r\nFlashSize=0x%x", gRegionAttrs.regionSize);
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_ls");
        return CRS_FAILURE;
    }
    CRS_FAT_t fat[MAX_FILES] = {0};
    Nvs_readFAT((fat));
    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    uint32_t strlen;
    int i = 0;
    //int numfiles = 0;
    int pageFileNum = 10; // number of files in a page
    bool moreFiles = false;

    int fileNum = MAX_FILES;
    if (page){
        fileNum = pageFileNum*page;
        if (fileNum>MAX_FILES){
            fileNum = MAX_FILES;
        }
        i = fileNum - pageFileNum;
    }
    while (i < fileNum)
    {
        if (fat[i].isExist == true)
        {
            if (nvsInit() != CRS_SUCCESS)
           {
                   CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_ls");
                   return CRS_FAILURE;
           }
            NVS_read(gNvsHandle,
                     (fat[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                     (void*) strlenStr, VARS_HDR_SZ_BYTES);
            sscanf(strlenStr, "%x", &strlen);
            if(strlen){
                strlen -= 1;
            }
            CLI_cliPrintf("\r\nName=%s Size=0x%x", fat[i].filename, strlen);
//            numfiles++;
            strlen = 0;
            memset(strlenStr, 0, VARS_HDR_SZ_BYTES);
        }

        i++;
    }

    if(i < MAX_FILES){
        if(fat[i].isExist==true){
            moreFiles = true;
        }
    }
//    if (){
//        CLI_cliPrintf("\r\nAvblFilesSlots=0x%x", gNumFiles - numfiles);
//    }
    if(moreFiles){
        nvsClose();

        return CRS_NEXT;
    }
    CLI_cliPrintf("\r\nMaxFileSize=0x%x", gRegionAttrs.sectorSize);
    CLI_cliPrintf("\r\nFlashSize=0x%x", gRegionAttrs.regionSize);
    CLI_cliPrintf("\r\nMaxFilesNum=0x%x", gNumFiles);
    nvsClose();

    return CRS_SUCCESS;
}



CRS_retVal_t Nvs_writeFile(char *filename, char *buff)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_writeFile");
        return CRS_FAILURE;
    }
    // currently set maximum filename length to 32 chars
    if (strlen(filename) > VARS_HDR_SZ_BYTES){
        return CRS_FAILURE;
    }
    char temp[1024] = { 0 };
    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    memcpy(temp, buff, strlen(buff));
//    strcat(temp, "\n");

    int i = 0;
    Bool isAppend = false;
    //looking for a exsiting filename
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i % FAT_CACHE_SZ].filename) == strlen(filename)
                && memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                          strlen(filename)) == 0)
        {
            isAppend = true;
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if ((memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0) && (strlen(filename)==strlen(FATcache[i % FAT_CACHE_SZ].filename)))
            {
                isAppend = true;
                break;
            }
        }
        i++;

    }

    //if no file already exist with the same name
    if (isAppend == false)
    {
        CRS_FAT_t fat[MAX_FILES] = {0};
        Nvs_readFAT((fat));
        i = 0;
        while (fat[i].isExist == true)
        {
            i++;
            if (i == MAX_FILES)
            {
                CLI_cliPrintf("no available slots!\r\n");
                nvsClose();

                return CRS_FAILURE;
            }
        }
        int2hex(strlen(temp), strlenStr);
        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, VARS_HDR_SZ_BYTES,
                  NVS_WRITE_ERASE);
        NVS_write(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (VARS_HDR_SZ_BYTES + 1),
                (void*) temp, strlen(temp), 0);
        memset(fat[i].filename, 0, FILENAME_SZ);
        memcpy(fat[i].filename, filename, strlen(filename));
//        fat[i].len = strlen(temp);
        fat[i].index = i + 1;
        fat[i].isExist = true;
        Nvs_writeFAT(fat);
        nvsClose();

        return CRS_SUCCESS;
    }
    //we append to the existing file
    else
    {
        char fileContent[4096] = { 0 };
        NVS_read(
                gNvsHandle,
                (FATcache[i % FAT_CACHE_SZ].index + gFAT_sector_sz)
                        * gRegionAttrs.sectorSize,
                (void*) strlenStr, VARS_HDR_SZ_BYTES);

        uint32_t strlenPrev = CLI_convertStrUint(strlenStr);
        if (strlenPrev + strlen(buff) > 4000)
        {
            nvsClose();

            return CRS_FAILURE;
        }
        NVS_read(
                gNvsHandle,
                ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                        + (VARS_HDR_SZ_BYTES + 1),
                (void*) fileContent, strlenPrev);
        memset(strlenStr, 0, VARS_HDR_SZ_BYTES);

        uint32_t newStrlen = strlenPrev + strlen(temp);

        int2hex(newStrlen, strlenStr);

        NVS_write(gNvsHandle,
                  (i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize,
                  (void*) strlenStr, VARS_HDR_SZ_BYTES,
                  NVS_WRITE_ERASE);
        strcat(fileContent, temp);
        size_t startFile = ((i + 1 + gFAT_sector_sz) * gRegionAttrs.sectorSize)
                + VARS_HDR_SZ_BYTES + 1;
        NVS_write(gNvsHandle, startFile, (void*) fileContent, newStrlen, 0);
//        fat[i].len += strlen(temp);
    }
    nvsClose();

    return CRS_SUCCESS;
}


CRS_retVal_t Nvs_cat(char *filename)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_catSegment");
        return CRS_FAILURE;
    }
    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    CRS_FAT_t fat[MAX_FILES] = {0};
    Nvs_readFAT((fat));
    int i = 0;
    while (i < MAX_FILES)
    {
        if ((memcmp(fat[i].filename, filename, strlen(filename)) == 0) && (strlen(filename)==strlen(fat[i].filename)) )
        {
            break;
        }
        i++;
    }
    if (i == MAX_FILES)
    {
        //CLI_cliPrintf("\r\nfile not found!\r\n");
        nvsClose();

        return CRS_FAILURE;
    }
    char fileContent[4096] = { 0 };
    NVS_read(gNvsHandle,
             (fat[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, VARS_HDR_SZ_BYTES);

    uint32_t strlen;
    sscanf(strlenStr, "%x", &strlen);
    size_t startFile = ((fat[i].index + gFAT_sector_sz)
            * gRegionAttrs.sectorSize) + (VARS_HDR_SZ_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) fileContent, strlen);
//    char line[50] = { 0 };
    const char s[2] = "\n";
    char *token;
    token = strtok((fileContent), s);
    while (token != NULL)
    {
        CLI_cliPrintf("\r\n%s", token);
        Task_sleep(100);
        token = strtok(NULL, s);
    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_catSegment(char *filename, uint32_t fileIndex, uint32_t readSize)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_catSegment");
        return CRS_FAILURE;
    }

    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    CRS_FAT_t fat[MAX_FILES] = {0};
    Nvs_readFAT((fat));
    int i = 0;
    while (i < MAX_FILES)
    {
        if ((memcmp(fat[i].filename, filename, strlen(filename)) == 0) && (strlen(filename)==strlen(fat[i].filename)))
        {
            break;
        }
        i++;
    }
    if (i == MAX_FILES)
    {
        //CLI_cliPrintf("\r\nfile not found!\r\n");
        nvsClose();

        return CRS_FAILURE;
    }
    char fileContent[4096] = { 0 };
    NVS_read(gNvsHandle,
             (fat[i].index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, VARS_HDR_SZ_BYTES);

    uint32_t stringlen;
    sscanf(strlenStr, "%x", &stringlen);
    size_t startFile = ((fat[i].index + gFAT_sector_sz)
            * gRegionAttrs.sectorSize) + (VARS_HDR_SZ_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) fileContent, stringlen);
//    char line[50] = { 0 };
//    const char s[2] = "\n";
//    char *token;
    char fileSegment[650] = { 0 };
    if((fileIndex+readSize)>4096){
        readSize = 4096 - fileIndex;
    }
    memcpy(fileSegment, &fileContent[fileIndex], readSize);
    if(strlen(fileContent) <= (fileIndex+readSize))
    {
        // nvs_write puts newline at end of file, so we remove it before printing
        fileSegment[strlen(fileSegment)-1] = 0;
    }
    CLI_cliPrintf("\r\n%s", fileSegment);
   //if()
//    token = strtok((fileSegment), s);
//    while (token != NULL)
//    {
//        CLI_cliPrintf("\r\n%s", token);
//        Task_sleep(100);
//        token = strtok(NULL, s);
//    }
    nvsClose();

    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_rm(char *filename)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_rm");
        return CRS_FAILURE;
    }
    CRS_FAT_t fat[MAX_FILES] = {0};
    Nvs_readFAT((fat));
    int i = 0;
    while (i < gNumFiles)
    {
        if ((memcmp(fat[i].filename, filename, strlen(filename)) == 0) && (strlen(filename)==strlen(fat[i].filename)))
        {
            break;
        }
        i++;
    }
    if (i == gNumFiles)
    {
        //CLI_cliPrintf("\r\nno such file\r\n");
        nvsClose();

        return CRS_FAILURE;
    }
    fat[i].isExist = false;
    memset(fat[i].filename, 0, FILENAME_SZ);
    Nvs_writeFAT(fat);
    nvsClose();

    //CLI_cliPrintf("\r\n%s deleted\r\n", filename);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_debug()
{
//    CRS_FAT_t *fat={0};
//
//    NVS_read(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t));
//
//    char fileContent[4096] = { 0 };
//    NVS_read(gNvsHandle,
//             ((fat)->index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
//             (void*) fileContent, (fat)->len);
//    readJson(fileContent);
//    int_fast16_t retStatus = NVS_write(gNvsHandle, (4096 * 2), (void* )buff, sizeof(buff),
//    NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
//    printStatus(retStatus);

//    char buffer[3000];
//    NVS_erase(gNvsHandle, 0, gRegionAttrs.sectorSize);
//    NVS_read(gNvsHandle, 0, (void*) buffer, sizeof(buffer));
//    CLI_cliPrintf("\r\nsize of fat struct: %d\r\nfat sector sz: %d",
//                  sizeof(CRS_FAT_t), gFAT_sector_sz);
    return CRS_SUCCESS;
}

CRS_retVal_t Nvs_format()
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_format");
        return CRS_FAILURE;
    }
    int_fast16_t retStatus = NVS_erase(gNvsHandle, 0, gRegionAttrs.regionSize);
    nvsClose();

    //printStatus(retStatus);
    if (retStatus==NVS_STATUS_SUCCESS ) {
        return CRS_SUCCESS;
    }else{
        return CRS_FAILURE;
    }

}

CRS_retVal_t Nvs_readFile(const char *filename, char *respLine)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_readFile");
        return CRS_FAILURE;
    }
    CRS_FAT_t fat;
    uint32_t i = 0;
//    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i % FAT_CACHE_SZ].filename) == strlen(filename)
                && memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                          strlen(filename)) == 0)
        {
//            fat = FATcache[i];
            memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if ((memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0)  && (strlen(filename)==strlen(FATcache[i % FAT_CACHE_SZ].filename)))
            {
                //            fat = FATcache[i];
                memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                break;
            }
        }
        i++;
    }

    if (i == MAX_FILES)
    {
        CRS_LOG(CRS_ERR, "File not found");
        nvsClose();

        return CRS_FAILURE;
    }
//    CLI_cliPrintf("filenamefat:%s, filenameme:%s, sizeme:0x%x", fat.filename, filename, strlen(filename));

    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    uint32_t strlen_f;
    NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, VARS_HDR_SZ_BYTES);
    sscanf(strlenStr, "%x", &strlen_f);
    size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + (VARS_HDR_SZ_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) respLine, strlen_f);
    nvsClose();

    return CRS_SUCCESS;
}



CRS_retVal_t Nvs_isFileExists(char *filename)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_isFileExists");
        return CRS_FAILURE;
    }
    CRS_FAT_t fat;
    uint32_t i = 0;
    //    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i % FAT_CACHE_SZ].filename) == strlen(filename)
                && memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                          strlen(filename)) == 0)
        {
            //            fat = FATcache[i];
            memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
            //            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if ((memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0)  && (strlen(filename)==strlen(FATcache[i % FAT_CACHE_SZ].filename)))
            {
                //            fat = FATcache[i];
                memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                break;
            }
        }
        i++;
    }

    if (i == MAX_FILES)
    {
        nvsClose();

        return CRS_FAILURE;
    }
    nvsClose();

    return CRS_SUCCESS;

}

CRS_retVal_t Nvs_readLine(char *filename, uint32_t lineNumber, char *respLine)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nnvsInit failed for Nvs_readLine");
        return CRS_FAILURE;
    }
    CRS_FAT_t fat;
    uint32_t i = 0;
    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if ((memcmp(FATcache[i].filename, filename, strlen(filename)) == 0) && (strlen(filename)==strlen(FATcache[i].filename)))
        {
            fat = FATcache[i];
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0)
        {
//            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }
        i++;
    }

    if (i == MAX_FILES)
    {
        CLI_cliPrintf("\r\nfile not found!\r\n");
        nvsClose();

        return CRS_FAILURE;
    }
    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    uint32_t strlen_f;
    NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, VARS_HDR_SZ_BYTES);
    sscanf(strlenStr, "%x", &strlen_f);
    char fileContent[4096] = { 0 };
    size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + (VARS_HDR_SZ_BYTES + 1);
    NVS_read(gNvsHandle, startFile, (void*) fileContent, strlen_f);

    i = 0;
    const char *s = "\r\n";
    char *token;
    token = strtok((fileContent), s);
    while (token != NULL)
    {
        i++;
        if (i == lineNumber)
        {
            memcpy(respLine, token, strlen(token));
            nvsClose();

            return CRS_SUCCESS;
        }
        token = strtok(NULL, s);
    }

    if (lineNumber > i)
    {
//        OsalPort_free(bufCache);
        memcpy(respLine, "FAILURE", CRS_NVS_LINE_BYTES);
        nvsClose();

        return CRS_EOF;
    }
    memcpy(respLine, "FAILURE", CRS_NVS_LINE_BYTES);
    nvsClose();

    return CRS_FAILURE;
}

char* Nvs_readFileWithMalloc(char *filename)
{
    if (nvsInit() != CRS_SUCCESS)
    {
        CRS_LOG(CRS_ERR,"\r\nNvs_readFileWithMalloc nvsInit failed");
        return NULL;
    }
//    CLI_printHeapStatus();
    CRS_FAT_t fat;
    char *respLine = NULL;
    uint32_t i = 0;
    //    CLI_cliPrintf("filenameme:%s, sizeme:0x%x", respLine, strlen(respLine));

    Nvs_readFATNumFiles(FATcache, 0, FAT_CACHE_SZ);
    while (i < MAX_FILES)
    {

        if (strlen(FATcache[i % FAT_CACHE_SZ].filename) == strlen(filename)
                && memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                          strlen(filename)) == 0)
        {
            //            fat = FATcache[i];
            memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
            break;
        }
        else if ((i % FAT_CACHE_SZ) == 0 && i != 0)
        {
            //            CLI_cliPrintf("\r\nNOT INTO CACHE!!!!!!!!!!!!!!\r\n");
            Nvs_readFATNumFiles(FATcache, i, FAT_CACHE_SZ);
        }

        if (i >= FAT_CACHE_SZ && i % FAT_CACHE_SZ == 0)
        {
            if ((memcmp(FATcache[i % FAT_CACHE_SZ].filename, filename,
                       strlen(filename)) == 0) && (strlen(filename)==strlen(FATcache[i % FAT_CACHE_SZ].filename)))
            {
                //            fat = FATcache[i];
                memcpy(&fat, &(FATcache[i % FAT_CACHE_SZ]), sizeof(CRS_FAT_t));
                break;
            }
        }
        i++;
    }

    if (i == MAX_FILES)
    {
        CRS_LOG(CRS_ERR, "File %s not found",filename);
        nvsClose();

        return NULL;
    }
    //    CLI_cliPrintf("filenamefat:%s, filenameme:%s, sizeme:0x%x", fat.filename, filename, strlen(filename));

    char strlenStr[VARS_HDR_SZ_BYTES] = { 0 };
    uint32_t strlen_f;
    NVS_read(gNvsHandle, (fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize,
             (void*) strlenStr, VARS_HDR_SZ_BYTES);
    sscanf(strlenStr, "%x", &strlen_f);
    size_t startFile = ((fat.index + gFAT_sector_sz) * gRegionAttrs.sectorSize)
            + (VARS_HDR_SZ_BYTES + 1);

    respLine = CRS_malloc(strlen_f + 100);
    if (respLine == NULL)
    {
        CRS_LOG(CRS_ERR, "Malloc failed");
        nvsClose();

        return NULL;

    }
    memset(respLine, 0, strlen_f + 100);
    NVS_read(gNvsHandle, startFile, (void*) respLine, strlen_f);
    nvsClose();

    return respLine;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/

static CRS_retVal_t nvsInit()
{
    if (gIsMoudleInit == true)
    {
        return CRS_SUCCESS;
    }
    NVS_Params nvsParams;
    NVS_Params_init(&nvsParams);

    gNvsHandle = NVS_open(FS_NVS_0, &nvsParams);

    if (gNvsHandle == NULL)
    {
        CLI_cliPrintf("\r\nNVS_open() failed");

        return (CRS_FAILURE);
    }
    /*
     * This will populate a NVS_Attrs structure with properties specific
     * to a NVS_Handle such as region base address, region size,
     * and sector size.
     */
    NVS_getAttrs(gNvsHandle, &gRegionAttrs);
    uint32_t numFiles = (gRegionAttrs.regionSize / gRegionAttrs.sectorSize);
    gFAT_sector_sz = (numFiles * sizeof(CRS_FAT_t)
            + (gRegionAttrs.sectorSize - 1)) / gRegionAttrs.sectorSize;
    gFAT_sector_sz++;
    gNumFiles = numFiles - gFAT_sector_sz;

    gIsMoudleInit = true;
    return CRS_SUCCESS;
}


static CRS_retVal_t nvsClose()
{
    gIsMoudleInit = false;

    if (gNvsHandle == NULL)
    {
        return CRS_SUCCESS;
    }
    NVS_close(gNvsHandle);
    gNvsHandle = NULL;
    return CRS_SUCCESS;
}


static CRS_retVal_t Nvs_readFAT(CRS_FAT_t *fat)
{
    NVS_read(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t) * gNumFiles);
    return CRS_SUCCESS;
}

static CRS_retVal_t Nvs_readFATNumFiles(CRS_FAT_t *fat, uint32_t start,
                                        uint32_t numFiles)
{
    int_fast16_t retStatus = NVS_read(gNvsHandle, start * sizeof(CRS_FAT_t),
                                      (void*) fat,
                                      sizeof(CRS_FAT_t) * numFiles);
    return CRS_SUCCESS;
}


static CRS_retVal_t Nvs_writeFAT(CRS_FAT_t *fat)
{
    NVS_write(gNvsHandle, 0, (void*) fat, sizeof(CRS_FAT_t) * gNumFiles,
    NVS_WRITE_ERASE);
    return CRS_SUCCESS;
}


//static void printStatus(int_fast16_t retStatus)
//{
//    if (retStatus == NVS_STATUS_SUCCESS)
//    {
//        CLI_cliPrintf("\r\nNVS_STATUS_SUCCESS");
//    }
//    if (retStatus == NVS_STATUS_ERROR)
//    {
//        CLI_cliPrintf("\r\nNVS_STATUS_ERROR");
//    }
//    if (retStatus == NVS_STATUS_INV_OFFSET)
//    {
//        CLI_cliPrintf("\r\nNVS_STATUS_INV_OFFSET");
//    }
//    if (retStatus == NVS_STATUS_INV_WRITE)
//    {
//        CLI_cliPrintf("\r\NVS_STATUS_INV_WRITE");
//    }
//    if (retStatus == NVS_STATUS_INV_ALIGNMENT)
//    {
//        CLI_cliPrintf("\r\NVS_STATUS_INV_ALIGNMENT");
//    }
//}
