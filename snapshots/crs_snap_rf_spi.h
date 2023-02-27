/*
 * crs_snap_rf.h
 *
 *  Created on: 18 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_SNAP_RF_SPI_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_SNAP_RF_SPI_H_
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
/* Driver Header files */
#include <ti/drivers/NVS.h>
/* Driver configuration */
#include "ti_drivers_config.h"

/******************************************************************************
 Function Prototypes
 *****************************************************************************/


CRS_retVal_t SPI_RF_uploadSnapRf(char *filename, uint32_t rfAddr,
                             uint32_t RfLineNum, CRS_chipMode_t chipMode,
                             CRS_nameValue_t *nameVals, bool isFromFlat);
//void SPI_RF_process(void);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_SNAP_RF_H_ */
