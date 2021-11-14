/*
 * crs_fpga.h
 *
 *  Created on: 10 Nov 2021
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_FPGA_H_
#define APPLICATION_CRS_CRS_FPGA_H_

#include <stdbool.h>
#include <stdint.h>

CRS_retVal_t  Fpga_init();
CRS_retVal_t Fpga_wrCommand(void * _buffer, size_t _size);
CRS_retVal_t Fpga_rdCommand(void * _buffer, size_t _size);




#endif /* APPLICATION_CRS_CRS_FPGA_H_ */
