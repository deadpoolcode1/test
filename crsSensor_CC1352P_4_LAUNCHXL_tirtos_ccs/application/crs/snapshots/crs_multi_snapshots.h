/*
 * crs_multi_snapshots.h
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_
#define APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_

#include "application/crs/crs_fpga.h"
#include "crs_snapshot.h"
#include "application/crs/crs.h"
#include "config_parsing.h"

CRS_retVal_t MultiFiles_multiFilesInit(void *sem);
void MultiFiles_process(void);
CRS_retVal_t MultiFiles_runMultiFiles(crs_package_t *package, CRS_chipType_t chipType, CRS_chipMode_t chipMode, FPGA_cbFn_t cbFunc);

#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_ */
