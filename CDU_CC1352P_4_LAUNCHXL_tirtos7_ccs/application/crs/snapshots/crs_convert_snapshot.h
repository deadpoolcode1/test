/*
 * crs_convert_snapshot.h
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_CONVERT_SNAPSHOT_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_CONVERT_SNAPSHOT_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_fpga_uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "mac/mac_util.h"
#include "application/crs/crs_cli.h"
#include "application/crs/crs_nvs.h"
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define CRS_NVS_LINE_BYTES 50
#define STAR_RD_LINE_TO_SEND 0x01
#define STAR_FINAL_LINE_TO_SEND 0x02

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Convert_rdParser(char *addr, char respLine[9][CRS_NVS_LINE_BYTES]);

CRS_retVal_t Convert_readLineRf(char *line,
                                char respLine[9][CRS_NVS_LINE_BYTES],
                                uint32_t LUT_line, CRS_chipMode_t chipMode);

CRS_retVal_t Convert_readLineDig(char *line,
                                 char respLine[9][CRS_NVS_LINE_BYTES],
                                 CRS_chipMode_t chipMode);

CRS_retVal_t Convert_convertStar(char *starLine, char *rdResp,
                                 char *parsedLine);
CRS_retVal_t Convert_readLineFpga(char *line,
                                  char respLine[9][CRS_NVS_LINE_BYTES]);
CRS_retVal_t Convert_applyLine(char respLine[9][CRS_NVS_LINE_BYTES],
                               uint32_t LUT_line, CRS_chipMode_t chipMode);

CRS_retVal_t Convert_rd(uint32_t addr, CRS_chipMode_t mode, uint32_t rfAddr,
                        char convertedResp[2][CRS_NVS_LINE_BYTES]);

CRS_retVal_t Convert_wr(uint32_t addr, uint32_t value, CRS_chipMode_t mode,
                        uint32_t rfAddr,
                        char convertedResp[2][CRS_NVS_LINE_BYTES]);
CRS_retVal_t Config_runConfigDirect();
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_CONVERT_SNAPSHOT_H_ */
