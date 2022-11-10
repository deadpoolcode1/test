/*
 * crs_flat_parser_spi.h
 *
 *  Created on: Nov 3, 2022
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_FLAT_PARSER_SPI_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_FLAT_PARSER_SPI_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_cli.h"
#include "application/crs/crs_nvs.h"
#include "application/crs/crs.h"
#include "crs_snap_rf.h"
#include <stdio.h>
#include <string.h>

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define MODULE_NAME_SZ 70
#define ADDR_SZ 20
#define CLASSID_SZ 12
#define SCRIPT_SZ 80
#define EXPECTEDVAL_SZ 20
#define FILEINFO_SZ 5
#define LUT_SZ 32
#define LINE_MESSAGE_SZ 100

typedef enum SPI_CRS_fileType
{
    SPI_SN, SPI_SC
} SPI_CRS_fileType_t;

typedef struct SPI_crs_discseqLine
{
    uint32_t index;
    char script[SCRIPT_SZ];
    char expectedVal[EXPECTEDVAL_SZ];
} SPI_crs_discseqLine_t;

typedef struct SPI_crs_fileInfo
{
    char name[FILENAME_SZ];
    SPI_CRS_fileType_t type;
    char LUTline[LUT_SZ];
    CRS_nameValue_t nameValues[NAME_VALUES_SZ];
} SPI_crs_fileInfo_t;

typedef struct SPI_crs_package
{
    uint32_t index;
    uint32_t numFileInfos;
    SPI_crs_fileInfo_t fileInfos[FILEINFO_SZ];
    char initScript[100];
} SPI_crs_package_t;

typedef struct SPI_CRS_invLine
{
    uint32_t order;
    char ClassID[CLASSID_SZ];
    char Name[MODULE_NAME_SZ];
    char Addr[ADDR_SZ];
    uint32_t DiscSeq;
    bool Mandatory;
    CRS_chipType_t ChipType;
    CRS_chipMode_t Flavor;
    uint32_t Package;
    char LineMessage [LINE_MESSAGE_SZ];
} SPI_CRS_invLine_t;

typedef struct SPI_crs_inv
{
    SPI_CRS_invLine_t invs[15];
    SPI_crs_package_t packages[15];
    SPI_crs_discseqLine_t disceqs[15];
} SPI_crs_inv_t;

typedef struct SPI_crs_box
{
    char name[50];
    SPI_crs_inv_t cu;
} SPI_crs_box_t;

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t SPI_Config_configInit(void *sem);
CRS_retVal_t SPI_Config_runConfigFile(char *filename);
CRS_retVal_t SPI_Config_runConfigFileDiscovery(char *filename);
CRS_retVal_t SPI_Config_getInvLine(char *invName, uint32_t lineNum,
                               char *fileContent, char *respLine);
CRS_retVal_t SPI_Config_getPackageLine(uint32_t lineNum, char *fileContent,
                                   char *respLine);
CRS_retVal_t SPI_Config_getDiscoveryLine(uint32_t lineNum, char *fileContent,
                                     char *respLine);
CRS_retVal_t SPI_Config_parsePackageLine(char *buff,
                                     SPI_crs_package_t *respStructPackage);
CRS_retVal_t SPI_Config_parseDiscoveryLine(char *buff,
                                       SPI_crs_discseqLine_t *respStructDiscseq);
CRS_retVal_t SPI_Config_parseInvLine(char *buff,
                                 SPI_CRS_invLine_t *respStructInventory);
CRS_retVal_t SPI_Config_runConfigFileLine(char *filename, uint32_t lineNum,
                                      char *fileInfos);
CRS_retVal_t SPI_Config_runConfigDirect(char *filename, char *type, char *fileInfos);

uint32_t SPI_getLutRegValue(uint32_t chipNumber, uint32_t lineNumber, uint32_t lutNumber, uint32_t regNumber);
CRS_retVal_t SPI_readRfRegs(uint32_t chipNumber, uint32_t lineNumber);
uint32_t SPI_getGlobalRegValue(uint32_t regIdx);
void SPI_Config_process(void);


#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_FLAT_PARSER_SPI_H_ */
