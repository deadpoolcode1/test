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
#include "cui.h"
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
static uint8_t gUartTxBuffer[TX_BUFF_SIZE] = {0};
static uint8_t gUartTxBufferIdx = 0;
static uint8_t gUartRxBuffer[1] = {0};
static FPGA_cbFn_t gCbFn = NULL;
static uint32_t gCommandSize = 0;

static bool gIsDoneReading = false;

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);



CRS_retVal_t  Fpga_init(FPGA_cbFn_t _cbFn)
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
//        gUartParams.writeMode = UART_MODE_CALLBACK;
        gUartParams.writeMode = UART_MODE_BLOCKING;

        gUartParams.writeDataMode = UART_DATA_BINARY;
        //gUartParams.writeCallback = UartWriteCallback;
        gUartParams.readReturnMode = UART_RETURN_FULL;
        gUartParams.baudRate = 115200;
        gUartParams.readEcho = UART_ECHO_OFF;
        gUartParams.readMode = UART_MODE_CALLBACK;
        gUartParams.readDataMode = UART_DATA_BINARY;
        gUartParams.readCallback = UartReadCallback;

        gUartHandle = UART_open(CONFIG_UART_1, &gUartParams);
        if (NULL == gUartHandle)
        {
            return CRS_FAILURE;
        }
        char *buffer[6]={"wr 0xb 0x1\r","wr 0x3 0x0\r","wr 0x3 0x1\r","wr 0x50 0x07a5a5\r", "wr 0x51 0x070000\r", "\r\r"};
        char* idx = buffer[5];

        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        memset(gUartRxBuffer, 0, 1);

        gCbFn = _cbFn;
        gUartTxBufferIdx = 0;
        UART_read(gUartHandle, gUartRxBuffer, 1);

        UART_write(gUartHandle, buffer[5], 1);

//        while (gIsDoneReading == false)
//        {
//            if (gIsDoneReading)
//            {
//                gIsDoneReading = false;
//                return CRS_SUCCESS;
//            }
//
//        }
//        gIsDoneReading = false;
        return CRS_SUCCESS;


      }
    return CRS_SUCCESS;

}

CRS_retVal_t  Fpga_close()
{
    if (gUartHandle == NULL)
        {
            return CRS_SUCCESS;
        }

    UART_close(gUartHandle);
    return CRS_SUCCESS;

}


CRS_retVal_t Fpga_wrCommand(void * _buffer, size_t _size,  FPGA_cbFn_t _cbFn)
{
    if (_buffer == NULL || _size == 0 || gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }
    gCbFn = _cbFn;

    memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
    memset(gUartRxBuffer, 0, 1);
    gCommandSize = _size;
    gUartTxBufferIdx = 0;
    UART_read(gUartHandle, gUartRxBuffer, 1);

    UART_write(gUartHandle, _buffer, _size);


    return CRS_SUCCESS;



}

CRS_retVal_t Fpga_rdCommand(void * _buffer, size_t _size,  FPGA_cbFn_t _cbFn)
{
    if (_buffer == NULL || _size == 0 || gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }

    gCbFn = _cbFn;


    memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
    memset(gUartRxBuffer, 0, 1);
    gCommandSize = _size;
    gUartTxBufferIdx = 0;
    UART_read(gUartHandle, gUartRxBuffer, 1);

    UART_write(gUartHandle, _buffer, _size);
    return CRS_SUCCESS;


}

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size)
{


    if (_size)
    {
//        gIsDoneReading = true;
//        gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*)_buf)[0];
//                  gUartTxBufferIdx++;
//                  memset(gUartRxBuffer, '\0', _size);
//                  UART_read(gUartHandle, gUartRxBuffer, 1);
//                  if (gUartTxBufferIdx > 4)
//                  {
//                      memset(gUartRxBuffer, '\0', _size);
//
//                  }
//
//        return 0;
          gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*)_buf)[0];
          gUartTxBufferIdx++;
          memset(gUartRxBuffer, '\0', _size);

          if (gUartTxBufferIdx <= 3)
          {
              UART_read(gUartHandle, gUartRxBuffer, 1);
              //gIsDoneReading = true;

          }
          else
          {
              int i = 0;
              for (i =0; i <= gUartTxBufferIdx; i++)
              {
                  if(memcmp(&(gUartTxBuffer[i]), "AP>", 3) == 0)
                  {
                      gIsDoneReading = true;
                      //CUI_cliPrintf(0, gUartTxBuffer);
                      FPGA_cbArgs_t cbArgs;
                      cbArgs.arg0 = i;
                      cbArgs.arg1 = gCommandSize;
                      cbArgs.arg3 = gUartTxBuffer;
                      gCbFn(cbArgs);
                      return 0;
                  }
              }
              UART_read(gUartHandle, gUartRxBuffer, 1);

          }

    }
    else
    {
        // Handle error or call to UART_readCancel()
        UART_readCancel(gUartHandle);
    }
}

