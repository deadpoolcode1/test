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
#include "crs_events.h"

typedef struct {
    uint32_t arg0;
    uint32_t arg1;
    char*    arg3;
} FPGA_cbArgs_t;

typedef void (*FPGA_cbFn_t)(const FPGA_cbArgs_t _cbArgs);


CRS_retVal_t  Fpga_init(FPGA_cbFn_t _cbFn);
CRS_retVal_t  Fpga_close();
CRS_retVal_t Fpga_wrCommand(void * _buffer, size_t _size, FPGA_cbFn_t _cbFn);
CRS_retVal_t Fpga_rdCommand(void * _buffer, size_t _size, FPGA_cbFn_t _cbFn);




#endif /* APPLICATION_CRS_CRS_FPGA_H_ */
