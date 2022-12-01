/*
 * crs_multi_snapshots.h
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_fpga.h"
#include "crs_snapshot.h"
#include "application/crs/crs.h"
#include "config_parsing.h"
/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t MultiFiles_multiFilesInit(void *sem);
CRS_retVal_t MultiFiles_runMultiFiles(crs_package_t *package,
                                      CRS_chipType_t chipType,
                                      CRS_chipMode_t chipMode,uint32_t rfAddr,
                                      FPGA_cbFn_t cbFunc);
void MultiFiles_process(void);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_ */
