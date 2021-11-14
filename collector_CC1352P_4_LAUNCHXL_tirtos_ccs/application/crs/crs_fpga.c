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

#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>

#include "ti_drivers_config.h"


#define TX_BUFF_SIZE 50

/*
 * [UART Specific Global Variables]
 */
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[TX_BUFF_SIZE];
static uint8_t gUartTxBufferIdx = 0;
static uint8_t gUartRxBuffer[1];

static bool gIsDoneReading = false;

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);



CRS_retVal_t  Fpga_init()
{
    if (gUartHandle != NULL)
    {
        return CRS_FAILURE;
    }

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

                   gUartHandle = UART_open(CONFIG_UART_1, &gUartParams);
                   if (NULL == gUartHandle)
                   {
                       return CRS_FAILURE;
                   }


      }
}

CRS_retVal_t Fpga_wrCommand(void * _buffer, size_t _size, char* res)
{
    if (_buffer == NULL || _size == 0 || gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }
    UART_write(gUartHandle, _buffer, _size);

    while (gIsDoneReading == false)
    {

    }
    gIsDoneReading = false;


}

CRS_retVal_t Fpga_rdCommand(void * _buffer, size_t _size, char* res);

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size)
{


    if (_size)
    {
          gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*)_buf)[0];
          gUartTxBufferIdx++;
          memset(gUartRxBuffer, '\0', _size);

          if (gUartTxBufferIdx <= 7)
          {
              UART_read(gUartHandle, gUartRxBuffer, sizeof(gUartRxBuffer));
              return CRS_SUCCESS;
          }

          if (memcmp(gUartTxBuffer[gUartTxBufferIdx- 1 - 7], "AP>", sizeof("AP>")) == 0)
          {
              gIsDoneReading = true;
              UART_read(gUartHandle, gUartRxBuffer, sizeof(gUartRxBuffer));
              return CRS_SUCCESS;
          }

    }
    else
    {
        // Handle error or call to UART_readCancel()
        UART_readCancel(gUartHandle);
    }
}

