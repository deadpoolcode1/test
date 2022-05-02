/*
 * crs_nvs.h
 *
 *  Created on: 20 ????? 2021
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_CRS_NVS_H_
#define APPLICATION_CRS_CRS_NVS_H_

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "crs_cli.h"
//#include "osal_port.h"
/* Driver Header files */
#include <ti/drivers/NVS.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define FILENAME_SZ 57
#define MAX_FILES 125
#define CRS_NVS_LINE_BYTES 50
#define FAT_CACHE_SZ 10
#define STRLEN_BYTES 32
#define MAX_LINE_CHARS 1024
typedef struct CRS_fat {
   char  filename[FILENAME_SZ];
   uint16_t index; //2 bytes
   uint16_t len; //2 bytes
   Bool isExist; //2 bytes
   uint8_t crc;
} CRS_FAT_t;


CRS_retVal_t Nvs_init(void *sem);
CRS_retVal_t Nvs_ls();
CRS_retVal_t Nvs_rm(char *filename);
CRS_retVal_t Nvs_writeFile(char *filename, char *buff);
CRS_retVal_t Nvs_writeTempFile(char *filename, char *buff);
CRS_retVal_t Nvs_JsonFile(char *filename, char *buff);
CRS_retVal_t Nvs_cat(char *filename);
CRS_retVal_t Nvs_debug();
CRS_retVal_t Nvs_format();
CRS_retVal_t Nvs_insertSpecificLine(char *filename, const char *line,
                                   uint32_t lineNum);

CRS_retVal_t Nvs_readLine(char *filename, uint32_t lineNumber,
                                char *respLine);
CRS_retVal_t Nvs_readFile(const char *filename, char *respLine);

CRS_retVal_t Nvs_treadVarsFile(char *vars, int file_type);
CRS_retVal_t Nvs_setVarsFile(char* vars, int file_type);
CRS_retVal_t Nvs_rmVarsFile(char* vars, int file_type);
CRS_retVal_t Nvs_createVarsFile(char* vars, int file_type);
CRS_retVal_t Nvs_isFileExists(char *filename);

char* Nvs_readFileWithMalloc(char *filename);



#endif /* APPLICATION_CRS_CRS_NVS_H_ */
