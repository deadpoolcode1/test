/*
 * crs_discovery.h
 *
 *  Created on: 4 Jan 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_DISCOVERY_H_
#define APPLICATION_CRS_CRS_DISCOVERY_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include "application/crs/crs_fpga_uart.h"
#include "crs.h"


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Discovery_init(void *sem);
CRS_retVal_t Discovery_discoverAll(FPGA_cbFn_t _cbFn);
CRS_retVal_t Discovery_discoverChip(CRS_chipType_t chipType, FPGA_cbFn_t _cbFn);
CRS_retVal_t Discovery_discoverMode(FPGA_cbFn_t _cbFn);
void Discovery_process(void);
#endif /* APPLICATION_CRS_CRS_DISCOVERY_H_ */
