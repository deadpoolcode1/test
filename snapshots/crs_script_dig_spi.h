/*
 * crs_script_dig.h
 *
 *  Created on: 7 Feb 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_SPI_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_SPI_H_
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
//#include "application/crs/crs_fpga.h"
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"
#include "application/util_timer.h"
#include "application/crs/crs_fpga_spi.h"
#include <ctype.h>

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t DigSPI_uploadSnapFpga(char *filename, CRS_chipMode_t chipMode,
                                CRS_nameValue_t *nameVals);
CRS_retVal_t DigSPI_uploadSnapDig(char *filename, CRS_chipMode_t chipMode,
                               uint32_t chipAddr, CRS_nameValue_t *nameVals);
//void DigSPI_process(void);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SCRIPT_DIG_SPI_H_ */
