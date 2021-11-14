/*
 * crs_fpga.c
 *
 *  Created on: 10 Nov 2021
 *      Author: epc_4
 */
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "crs_fpga.h"
#include <ti/drivers/dpl/SemaphoreP.h>

/*
 * [UART Specific Global Variables]
 */
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
#ifndef CUI_MIN_FOOTPRINT
static uint8_t gUartTxBuffer[CUI_NUM_UART_CHARS];
static uint8_t gUartTxBufferIdx = 0;
static uint8_t gUartRxBuffer[1];
#endif


CRS_retVal_t  Fpga_init()
{
    {
                   // General UART setup
                   UART_init();
                   UART_Params_init(&gUartParams);
                   gUartParams.baudRate = 115200;
                   gUartParams.writeMode     = UART_MODE_BLOCKING;
                   gUartParams.writeDataMode = UART_DATA_BINARY;
                   //gUartParams.writeCallback = UartWriteCallback;

                   gUartParams.readMode      = UART_MODE_CALLBACK;
                   gUartParams.readDataMode  = UART_DATA_BINARY;
                   gUartParams.readCallback  = UartReadCallback;

                   gUartHandle = UART_open(CONFIG_DISPLAY_UART, &gUartParams);
                   if (NULL == gUartHandle)
                   {
                       return CUI_FAILURE;
                   }


               }
}
CRS_retVal_t Fpga_wrCommand(void * _buffer, size_t _size)
{

}
CRS_retVal_t Fpga_rdCommand(void * _buffer, size_t _size);

