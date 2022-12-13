/*
 * crs_fpga.h
 *
 *  Created on: 10 Nov 2021
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_FPGA_UART_H_
#define APPLICATION_CRS_CRS_FPGA_UART_H_
/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "crs.h"
#include "string.h"


/******************************************************************************
 Constants and definitions
 *****************************************************************************/
typedef struct {
    uint32_t arg0;
    uint32_t arg1;
    char*    arg3;
} FPGA_cbArgs_t;

typedef void (*FPGA_cbFn_t)(const FPGA_cbArgs_t _cbArgs);

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t  Fpga_UART_initSem(void *sem);
CRS_retVal_t  Fpga_UART_init(FPGA_cbFn_t _cbFn);
CRS_retVal_t Fpga_UART_writeMultiLine(char *line, FPGA_cbFn_t _cbFn);
CRS_retVal_t  Fpga_UART_close();
CRS_retVal_t Fpga_UART_wrCommand(void * _buffer, size_t _size, FPGA_cbFn_t _cbFn);
CRS_retVal_t Fpga_UART_rdCommand(void * _buffer, size_t _size, FPGA_cbFn_t _cbFn);
CRS_retVal_t Fpga_UART_transparentOpen();
CRS_retVal_t Fpga_UART_transparentClose();
CRS_retVal_t Fpga_UART_setPrint(bool isToPrint);
CRS_retVal_t Fpga_UART_transparentWrite(void * _buffer, size_t _size);
CRS_retVal_t Fpga_UART_writeCommand(void * _buffer, size_t _size, FPGA_cbFn_t _cbFn );
CRS_retVal_t  Fpga_UART_isOpen();
CRS_retVal_t Fpga_UART_readMultiLine(char *line, FPGA_cbFn_t _cbFn);
void Fpga_UART_process(void);
void FPGA_UART_setFpgaClock(uint32_t fpgaTime);
CRS_retVal_t Fpga_UART_writeMultiLineNoPrint(char *line, FPGA_cbFn_t _cbFn);


#endif /* APPLICATION_CRS_CRS_FPGA_UART_H_ */
