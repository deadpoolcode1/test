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
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t Fpga_tmpInit();
CRS_retVal_t Fpga_tmpWriteMultiLine(char *line, FPGA_cbFn_t _cbFn);
CRS_retVal_t FPGA_getValFromBuf(uint8_t *buf, uint32_t* val);


#endif /* APPLICATION_CRS_CRS_TMP_H_ */
