/*
 * config_parsing.h
 *
 *  Created on: 5 ????? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CONFIG_PARSING_H_
#define APPLICATION_CRS_SNAPSHOTS_CONFIG_PARSING_H_

#include "crs_cli.h"
#include "crs_nvs.h"
#include "crs.h"
#include "crs_fpga.h"
#include "crs_snap_rf.h"
#include <stdio.h>
#include <string.h>
//#define FILENAME_SZ 50
#define NAME_SZ 20
#define ADDR_SZ 20
#define CLASSID_SZ 12
#define SCRIPT_SZ 80
#define EXPECTEDVAL_SZ 20
#define FILEINFO_SZ 5
#define LUT_SZ 32

typedef enum CRS_fileType
{
    SN, SC
} CRS_fileType_t;

typedef struct crs_discseqLine
{
    uint32_t index;
    char script[SCRIPT_SZ];
    char expectedVal[EXPECTEDVAL_SZ];
} crs_discseqLine_t;

typedef struct crs_fileInfo
{
    char name[FILENAME_SZ];
    CRS_fileType_t type;
    char LUTline[LUT_SZ];
    CRS_nameValue_t nameValues[NAME_VALUES_SZ];
} crs_fileInfo_t;

typedef struct crs_package
{
    uint32_t index;
    uint32_t numFileInfos;
    crs_fileInfo_t fileInfos[FILEINFO_SZ];
    char initScript[100];
} crs_package_t;

typedef struct CRS_invLine
{
    uint32_t order;
    char ClassID[CLASSID_SZ];
    char Name[NAME_SZ];
    char Addr[ADDR_SZ];
    uint32_t DiscSeq;
    bool Mandatory;
    CRS_chipType_t ChipType;
    CRS_chipMode_t Flavor;
    uint32_t Package;
} CRS_invLine_t;

typedef struct crs_inv
{
    CRS_invLine_t invs[15];
    crs_package_t packages[15];
    crs_discseqLine_t disceqs[15];
} crs_inv_t;

typedef struct crs_box
{
    char name[50];
    crs_inv_t cu;
} crs_box_t;

CRS_retVal_t Config_configInit(void *sem);
void Config_process(void);
CRS_retVal_t Config_runConfigFile(char *filename, FPGA_cbFn_t cbFunc);
CRS_retVal_t Config_getInvLine(char *invName, uint32_t lineNum, char *fileContent,
                        char *respLine);
CRS_retVal_t Config_getPackageLine(uint32_t lineNum, char *fileContent,
                            char *respLine);
CRS_retVal_t Config_getDiscoveryLine(uint32_t lineNum, char *fileContent,
                              char *respLine);

//CRS_retVal_t Config_getInvLine(char *invName, uint32_t lineNum, char *fileContent,
//                        char *respLine);
CRS_retVal_t Config_parsePackageLine(char *buff, crs_package_t *respStructPackage);
CRS_retVal_t Config_parseDiscoveryLine(char *buff, crs_discseqLine_t* respStructDiscseq);
CRS_retVal_t Config_parseInvLine(char *buff, CRS_invLine_t *respStructInventory);
CRS_retVal_t Config_runConfigFileLine(char *filename, uint32_t lineNum, char* fileInfos, FPGA_cbFn_t cbFunc);
CRS_retVal_t Config_runConfigDirect(char *filename,char *type,char* fileInfos ,FPGA_cbFn_t cbFunc);

#endif /* APPLICATION_CRS_SNAPSHOTS_CONFIG_PARSING_H_ */
