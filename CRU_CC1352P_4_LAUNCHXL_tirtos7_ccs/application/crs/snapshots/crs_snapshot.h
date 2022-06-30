/*
 * crs_snapshot.h
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SNAPSHOT_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SNAPSHOT_H_
/******************************************************************************
 Includes
 *****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "crs_convert_snapshot.h"
#include "mac/mac_util.h"
#include "application/crs/crs_cli.h"
#include "application/crs/crs_nvs.h"
#include "application/crs/crs_fpga.h"
/* Driver Header files */
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t SnapInit(void *sem);
void Snap_process(void);
CRS_retVal_t Snap_uploadSnapRf(char *filename, uint32_t rfAddr,
                               uint32_t LUTLineNum, CRS_chipMode_t chipMode,
                               CRS_nameValue_t *nameVals, FPGA_cbFn_t cbFunc);
CRS_retVal_t Snap_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                                FPGA_cbFn_t cbFunc);

CRS_retVal_t Snap_uploadSnapRaw(char *filename, CRS_chipMode_t chipMode,
                                FPGA_cbFn_t cbFunc);
CRS_retVal_t Snap_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                 FPGA_cbFn_t cbFunc);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SNAPSHOT_H_ */
