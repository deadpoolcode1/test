/*
 * crs_script_dig.h
 *
 *  Created on: 7 Feb 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_fpga_uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "crs_convert_snapshot.h"
#include "mac/mac_util.h"
#include "application/crs/crs_cli.h"
#include "application/crs/crs_nvs.h"
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"
#include "application/util_timer.h"
#include <ctype.h>

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t DigInit(void *sem);

CRS_retVal_t DIG_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                CRS_nameValue_t *nameVals, FPGA_cbFn_t cbFunc);
CRS_retVal_t DIG_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                               uint32_t chipNumber, CRS_nameValue_t *nameVals,
                               FPGA_cbFn_t cbFunc);
void DIG_process(void);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_H_ */
