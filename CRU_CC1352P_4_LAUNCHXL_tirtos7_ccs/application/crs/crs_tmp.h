/*
 * crs_tmp.h
 *
 *  Created on: 7 Aug 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_TMP_H_
#define APPLICATION_CRS_CRS_TMP_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"
#include "crs_fpga.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SPI_MSG_LENGTH  (10)
typedef void (*FPGA_tmpCbFn_t)(void);
/******************************************************************************
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t Fpga_tmpInit();
CRS_retVal_t Fpga_tmpWriteMultiLine(char *line, uint32_t* rsp);
CRS_retVal_t FPGA_getValFromBuf(uint8_t *buf, uint32_t* val);


#endif /* APPLICATION_CRS_CRS_TMP_H_ */