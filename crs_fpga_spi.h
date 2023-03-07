/*
 * crs_fpga_spi.h
 *
 *  Created on: 7 Aug 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_FPGA_SPI_H_
#define APPLICATION_CRS_CRS_FPGA_SPI_H_

/******************************************************************************
 Includes
 *****************************************************************************/
//#include "application/crs/crs_fpga_uart.h"
#include "crs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SPI_MSG_LENGTH  (10)
/******************************************************************************
 Function Prototypes
 *****************************************************************************/

CRS_retVal_t Fpga_SPI_Init();
CRS_retVal_t Fpga_SPI_WriteMultiLine(char *line, uint32_t* rsp);
CRS_retVal_t Fpga_SPI_GetValFromBuf(uint8_t *buf, uint32_t* val);


#endif /* APPLICATION_CRS_CRS_FPGA_SPI_H_ */
