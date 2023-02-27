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
#include "application/crs/crs_fpga_uart.h"
#include "application/crs/crs_fpga_spi.h"
#include "application/crs/crs.h"
#include "application/crs/snapshots/crs_flat_parser_spi.h"
/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t MultiFilesSPI_runMultiFiles(SPI_crs_package_t *package,
                                      uint32_t chipAddr,
                                      CRS_chipType_t chipType,
                                      CRS_chipMode_t chipMode);
//void MultiFilesSPI_process(void);
#endif /* APPLICATION_CRS_SNAPSHOTS_CRS_MULTI_SNAPSHOTS_H_ */
