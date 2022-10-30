/*
 * crs_fpga.c
 *
 *  Created on: 10 Nov 2021
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOCC26XX.h>

#include "mac/mac_util.h"
#include "application/util_timer.h"
#include "crs_fpga.h"
#include "ti_drivers_config.h"
#include "crs_cli.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define FPGA_WRITE_TIMEOUT_VALUE 10

#define LINE_SIZE 30

#define TX_BUFF_SIZE 100
#define UART_WRITE_BUFF_SIZE 50

#define FPGA_OPEN_EV 0x1
#define FPGA_TRANSPARENT_EV 0x2
#define FPGA_WRITE_LINES_EV 0x4
#define FPGA_OPEN_FAIL_EV 0x8
#define FPGA_READ_LINES_EV 0x10

/******************************************************************************
 Local variables
 *****************************************************************************/
static Clock_Struct fpgaClkStruct;
static Clock_Handle fpgaClkHandle;
/*
 * [UART Specific Global Variables]
 */
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[TX_BUFF_SIZE] = { 0 };
static uint8_t gUartTxBufferIdx = 0;
static uint8_t gUartRxBuffer[1] = { 0 };
static FPGA_cbFn_t gCbFn = NULL;
static FPGA_cbFn_t gCbMultiLineFn = NULL;
static FPGA_cbFn_t gCbFpgaOpenFn = NULL;

static uint32_t gCommandSize = 0;

static uint8_t gWriteNowBuff[UART_WRITE_BUFF_SIZE];
static uint8_t gWriteWaitingBuff[UART_WRITE_BUFF_SIZE];

static uint32_t gWriteNowBuffIdx = 0;
static uint32_t gWriteNowBuffSize = 0;
static uint32_t gWriteWaitingBuffIdx = 0;

//static uint32_t gWriteNowBuffTotalSize = 0;

static volatile bool gIsDoneWriting = true;
static volatile bool gIsDoneFilling = false;

//static bool gIsDoneReading = false;
static volatile bool gIsTransparentBridge = false;

static uint32_t gLineNumber = 0;
static uint32_t gNumLines = 0;

static char gFileToUpload[LINE_SIZE][LINE_SIZE] = { 0 };
static volatile bool gIsUploadingSnapshot = false;
static volatile bool gIsDoneUploadingLine = false;

static uint16_t Fpga_events = 0;

static Semaphore_Handle collectorSem;

static bool gIsRunnigLines = false;
static bool gIsToPrint = true;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);
static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size);
static void processFpgaTimeoutCallback(UArg a0);
static CRS_retVal_t FPGA_writeString(void *_buffer, size_t _size);
static void fpgaTransparentCliParsingCallback(const FPGA_cbArgs_t _cbArgs);
static void fpgaOpenParsingCallback(const FPGA_cbArgs_t _cbArgs);
//static void fpgaReadParsingCallback(const FPGA_cbArgs_t _cbArgs);
//static void fpgaWriteParsingCallback(const FPGA_cbArgs_t _cbArgs);
//static CRS_retVal_t fpgaParseCbRsp(char *rsp, uint32_t rspSize);
static void fpgaMultiLineWriteCallback(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t Fpga_getLine(char *line);
static void fpgaMultiLineReadCallback(const FPGA_cbArgs_t _cbArgs);

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Fpga_initSem(void *sem)
{
    collectorSem = sem;
    fpgaClkHandle = UtilTimer_construct(&fpgaClkStruct,
                                        processFpgaTimeoutCallback,
                                        FPGA_WRITE_TIMEOUT_VALUE,
                                        0,
                                        false,
                                        0);
    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_init(FPGA_cbFn_t _cbFn)
{
#ifdef CRS_TMP_SPI

    return CRS_FAILURE;
#endif

    if (gUartHandle != NULL)
    {
        return CRS_FAILURE;
    }

    #ifdef CRS_CB
        GPIO_setConfig(CONFIG_GPIO_TDD_SWITCH,  GPIO_CFG_OUT_STD | GPIO_CFG_OUT_STR_MED | GPIO_CFG_OUT_LOW);
    #endif


    {
        // General UART setup
        UART_init();
        UART_Params_init(&gUartParams);
        gUartParams.baudRate = 115200;
//        gUartParams.writeMode = UART_MODE_CALLBACK;
        gUartParams.writeMode = UART_MODE_CALLBACK;
        gUartParams.writeCallback = UartWriteCallback;

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
//        char *buffer[6] = { "wr 0xb 0x1\r", "wr 0x3 0x0\r", "wr 0x3 0x1\r",
//                            "wr 0x50 0x07a5a5\r", "wr 0x51 0x070000\r", "\r\r" };
//        char *idx = buffer[5];

        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        memset(gUartRxBuffer, 0, 1);

        gCbFn = fpgaOpenParsingCallback;
        gUartTxBufferIdx = 0;
        if (_cbFn != NULL)
        {
            gCbFpgaOpenFn = _cbFn;
        }
        UART_read(gUartHandle, gUartRxBuffer, 1);
        gIsRunnigLines = true;

        FPGA_setFpgaClock(20);
        FPGA_writeString("rd 0x51\r", strlen("rd 0x51\r"));
//        UART_write(gUartHandle, buffer[5], 1);

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


}

CRS_retVal_t Fpga_isOpen()
{
    if (gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_close()
{
    if (gUartHandle == NULL)
    {
        return CRS_SUCCESS;
    }

    UART_close(gUartHandle);
    gUartHandle = NULL;
    return CRS_SUCCESS;

}

CRS_retVal_t Fpga_writeMultiLineNoPrint(char *line, FPGA_cbFn_t _cbFn)
{
    gIsToPrint = false;
    Fpga_writeMultiLine(line, _cbFn);
    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_writeMultiLine(char *line, FPGA_cbFn_t _cbFn)
{
    if (line == NULL || _cbFn == NULL || gUartHandle == NULL || gIsRunnigLines == true)
    {
        FPGA_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = (char *)gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }


    gIsRunnigLines = true;
    gCbMultiLineFn = _cbFn;
    int i;
    gNumLines = 0;
    for (i = 0; i < LINE_SIZE; i++)
    {
        memset(gFileToUpload[i], 0, LINE_SIZE);
    }

    if (line[0] == '\r' && line[1] == 0)
    {
        gNumLines = 1;

        gIsUploadingSnapshot = true;
        gLineNumber = 1;

        Fpga_writeCommand(line, strlen(line), fpgaMultiLineWriteCallback);
        return CRS_SUCCESS;
    }

    const char d[2] = "\n";

    char *token2 = strtok((line), d);

    memcpy(gFileToUpload[0], token2, strlen(token2));

    token2 = strtok(NULL, d);
    gNumLines = 1;
    while ((token2 != NULL) && (strlen(token2) != 0))
    {
        memcpy(gFileToUpload[gNumLines], token2, strlen(token2));
        token2 = strtok(NULL, d);
        gNumLines++;
    }
    gLineNumber = 0;
    char lineToSend[100] = { 0 };
    memset(lineToSend, 0, LINE_SIZE);
    CRS_retVal_t rspStatus = Fpga_getLine(lineToSend);

    if (rspStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nFinished sending 0x%x lines", gLineNumber);
        CLI_startREAD();
        return CRS_SUCCESS;
    }

    gIsUploadingSnapshot = true;
    gLineNumber++;
    uint32_t sz = strlen(lineToSend);
    if (lineToSend[sz - 1] == '\r')
    {
        rspStatus = Fpga_writeCommand(lineToSend, strlen(lineToSend),
                                      fpgaMultiLineWriteCallback);
    }
    else
    {
        lineToSend[strlen(lineToSend)] = '\r';
        lineToSend[sz + 1] = 0;
        rspStatus = Fpga_writeCommand(lineToSend, strlen(lineToSend),
                                      fpgaMultiLineWriteCallback);

    }
    return CRS_SUCCESS;

}

CRS_retVal_t Fpga_readMultiLine(char *line, FPGA_cbFn_t _cbFn)
{
    if (line == NULL || _cbFn == NULL || gUartHandle == NULL || gIsRunnigLines == true)
    {
        FPGA_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 =(char *) gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    gIsRunnigLines = true;

    gCbMultiLineFn = _cbFn;
    int i;
    gNumLines = 0;
    for (i = 0; i < LINE_SIZE; i++)
    {
        memset(gFileToUpload[i], 0, LINE_SIZE);
    }

    if (line[0] == '\r' && line[1] == 0)
    {
        gNumLines = 1;

        gIsUploadingSnapshot = true;
        gLineNumber = 1;

        Fpga_writeCommand(line, strlen(line), fpgaMultiLineReadCallback);
        return CRS_SUCCESS;
    }

    const char d[2] = "\n";

    char *token2 = strtok((line), d);

    memcpy(gFileToUpload[0], token2, strlen(token2));

    token2 = strtok(NULL, d);
    gNumLines = 1;
    while ((token2 != NULL) && (strlen(token2) != 0))
    {
        memcpy(gFileToUpload[gNumLines], token2, strlen(token2));
        token2 = strtok(NULL, d);
        gNumLines++;
    }
    gLineNumber = 0;
    char lineToSend[100] = { 0 };
    memset(lineToSend, 0, LINE_SIZE);
    CRS_retVal_t rspStatus = Fpga_getLine(lineToSend);

    if (rspStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nFinished sending 0x%x lines", gLineNumber);
        CLI_startREAD();
        return CRS_SUCCESS;
    }

    gIsUploadingSnapshot = true;
    gLineNumber++;
    uint32_t sz = strlen(lineToSend);
    if (lineToSend[sz - 1] == '\r')
    {
        rspStatus = Fpga_writeCommand(lineToSend, strlen(lineToSend),
                                      fpgaMultiLineReadCallback);
    }
    else
    {
        lineToSend[strlen(lineToSend)] = '\r';
        lineToSend[sz + 1] = 0;
        rspStatus = Fpga_writeCommand(lineToSend, strlen(lineToSend),
                                      fpgaMultiLineReadCallback);

    }
    return CRS_SUCCESS;

}

CRS_retVal_t Fpga_writeCommand(void *_buffer, size_t _size, FPGA_cbFn_t _cbFn)
{
    if (_buffer == NULL || _size == 0 || gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }

    gCbFn = _cbFn;

    memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
    //memset(gUartRxBuffer, 0, 1);
    gCommandSize = _size;
    gUartTxBufferIdx = 0;

    memset(gUartRxBuffer, 0, 1);
    //UART_read(gUartHandle, gUartRxBuffer, 1);
    FPGA_writeString(_buffer, _size);
    UART_read(gUartHandle, gUartRxBuffer, 1);

//    UART_write(gUartHandle, _buffer, _size);
    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_transparentWrite(void *_buffer, size_t _size)
{
    if (_buffer == NULL || _size == 0 || gUartHandle == NULL)
    {
        return CRS_FAILURE;
    }

    gCbFn = fpgaTransparentCliParsingCallback;
    gIsRunnigLines = true;

    memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
    //memset(gUartRxBuffer, 0, 1);
    gCommandSize = _size;
    gUartTxBufferIdx = 0;
    //UART_read(gUartHandle, gUartRxBuffer, 1);
    FPGA_writeString(_buffer, _size);
//    UART_write(gUartHandle, _buffer, _size);
    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_transparentOpen()
{
    gIsTransparentBridge = true;
    memset(gUartRxBuffer, 0, 1);
    UART_read(gUartHandle, gUartRxBuffer, 1);
    return CRS_SUCCESS;
}

CRS_retVal_t Fpga_transparentClose()
{
    gIsTransparentBridge = false;
    UART_readCancel(gUartHandle);
    return CRS_SUCCESS;
}

void Fpga_process(void)
{

    if (Fpga_events & FPGA_OPEN_FAIL_EV)
    {
        gIsRunnigLines = false;

        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        //memset(gUartRxBuffer, 0, 1);
        gUartTxBufferIdx = 0;
        Fpga_close();
        if (gCbFpgaOpenFn != NULL)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);

            CRS_LOG(CRS_DEBUG, "fpga failed to open");

            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 =(char *) gUartTxBuffer;

            gCbFpgaOpenFn(cbArgs);
            gCbFpgaOpenFn = NULL;
        }
        else
        {

            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();

        }
        Util_clearEvent(&Fpga_events, FPGA_OPEN_FAIL_EV);
    }

    if (Fpga_events & FPGA_OPEN_EV)
    {
        gIsRunnigLines = false;

        //CLI_cliPrintf("%s", gUartTxBuffer);
        UtilTimer_stop(&fpgaClkStruct);
        if (gCbFpgaOpenFn != NULL)
        {
            CRS_LOG(CRS_DEBUG, "FINISHED script");

            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 =(char *) gUartTxBuffer;

            gCbFpgaOpenFn(cbArgs);
            gCbFpgaOpenFn = NULL;
        }
        else
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
            CLI_startREAD();
        }

        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        //memset(gUartRxBuffer, 0, 1);
        gUartTxBufferIdx = 0;
        Util_clearEvent(&Fpga_events, FPGA_OPEN_EV);
    }

    if (Fpga_events & FPGA_TRANSPARENT_EV)
    {
        gIsRunnigLines = false;

        CLI_cliPrintf("%s", gUartTxBuffer);
        CLI_startREAD();
        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        gUartTxBufferIdx = 0;
        UART_read(gUartHandle, gUartRxBuffer, 1);

        Util_clearEvent(&Fpga_events, FPGA_TRANSPARENT_EV);
    }

    if (Fpga_events & FPGA_WRITE_LINES_EV)
    {

//        if (gUartTxBuffer[0] == 'r')
#ifndef CLI_SENSOR
        if (gIsToPrint == true)
        {
            CLI_cliPrintf("%s", gUartTxBuffer);
        }
#endif
        char line[50] = { 0 };

        CRS_retVal_t rspStatus = Fpga_getLine(line);
        if (rspStatus != CRS_SUCCESS)
        {
//            CLI_cliPrintf("\r\nfinished sending: 0x%x", gLineNumber);
            gIsRunnigLines = false;
            gIsToPrint = true;
            Util_clearEvent(&Fpga_events, FPGA_WRITE_LINES_EV);

            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 =(char *) gUartTxBuffer;
            gCbMultiLineFn(cbArgs);
            return;
        }

        gLineNumber++;

        uint32_t sz = strlen(line);
        if ((sz == 1 && line[0] == '\r') || (line[strlen(line) - 1] == '\r'))
        {
            rspStatus = Fpga_writeCommand(line, strlen(line),
                                          fpgaMultiLineWriteCallback);
        }
        else
        {
            line[strlen(line)] = '\r';
            line[sz + 1] = 0;
            rspStatus = Fpga_writeCommand(line, strlen(line),
                                          fpgaMultiLineWriteCallback);

        }

        if (rspStatus != CRS_SUCCESS)
        {
            gIsRunnigLines = false;
            gIsToPrint = true;
            Util_clearEvent(&Fpga_events, FPGA_WRITE_LINES_EV);
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 =(char *) gUartTxBuffer;
            gCbMultiLineFn(cbArgs);
            return;
        }

        Util_clearEvent(&Fpga_events, FPGA_WRITE_LINES_EV);
    }

    if (Fpga_events & FPGA_READ_LINES_EV)
    {
        //        if (gUartTxBuffer[0] == 'r')
        CLI_cliPrintf("%s", gUartTxBuffer);
        char line[50] = { 0 };

        CRS_retVal_t rspStatus = Fpga_getLine(line);
        if (rspStatus != CRS_SUCCESS)
        {
            //            CLI_cliPrintf("\r\nfinished sending: 0x%x", gLineNumber);
            gIsRunnigLines = false;

            Util_clearEvent(&Fpga_events, FPGA_READ_LINES_EV);

            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 =(char *) gUartTxBuffer;
            gCbMultiLineFn(cbArgs);
            return;
        }

        gLineNumber++;

        uint32_t sz = strlen(line);
        if ((sz == 1 && line[0] == '\r') || (line[strlen(line) - 1] == '\r'))
        {
            rspStatus = Fpga_writeCommand(line, strlen(line),
                                          fpgaMultiLineReadCallback);
        }
        else
        {
            line[strlen(line)] = '\r';
            line[sz + 1] = 0;
            rspStatus = Fpga_writeCommand(line, strlen(line),
                                          fpgaMultiLineReadCallback);

        }

        if (rspStatus != CRS_SUCCESS)
        {
            gIsRunnigLines = false;
            gIsToPrint = true;
            Util_clearEvent(&Fpga_events, FPGA_READ_LINES_EV);
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            FPGA_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
            cbArgs.arg3 = (char *)gUartTxBuffer;
            gCbMultiLineFn(cbArgs);
            return;
        }

        Util_clearEvent(&Fpga_events, FPGA_READ_LINES_EV);
    }
}

void FPGA_setFpgaClock(uint32_t fpgaTime)
{
    /* Stop the Tracking timer */
    if (UtilTimer_isActive(&fpgaClkStruct) == true)
    {
        UtilTimer_stop(&fpgaClkStruct);
    }

    if (fpgaTime)
    {
        /* Setup timer */
        UtilTimer_setTimeout(fpgaClkHandle, fpgaTime);
        UtilTimer_start(&fpgaClkStruct);
    }
}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t Fpga_getLine(char *line)
{
    if (gNumLines <= gLineNumber)
    {
        return CRS_FAILURE;
    }
    memcpy(line, gFileToUpload[gLineNumber], LINE_SIZE);
    return CRS_SUCCESS;
}

static CRS_retVal_t FPGA_writeString(void *_buffer, size_t _size)
{
    /*
     * Since the UART driver is in Callback mode which is non blocking.
     *  If UART_write is called before a previous call to UART_write
     *  has completed it will not be printed. By taking a quick
     *  nap we can attempt to perform the subsequent write. If the
     *  previous call still hasn't finished after this nap the write
     *  will be skipped as it would have been before.
     */

    //Error if no buffer
    if ((gUartHandle == NULL))
    {
        return CRS_UART_FAILURE;
    }

    if (_buffer == NULL)
    {
        return CRS_SUCCESS;

    }

    if (gIsDoneWriting == true)
    {
        if (gWriteWaitingBuffIdx + _size < UART_WRITE_BUFF_SIZE)
        {
            memcpy(&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
            gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;
        }

        gWriteNowBuffSize = gWriteWaitingBuffIdx;

        memset(gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy(gWriteNowBuff, gWriteWaitingBuff, gWriteWaitingBuffIdx);
        memset(gWriteWaitingBuff, 0, gWriteWaitingBuffIdx);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
//        gWriteNowBuffTotalSize = gWriteNowBuffSize;
        UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);

        return CRS_SUCCESS;

    }
    else
    {
//        if (gWriteWaitingBuffIdx + _size >= UART_WRITE_BUFF_SIZE)
//         {
//            if(memcmp(CLI_PROMPT, _buffer, _size) == 0)
//            {
//                gIsDoneFilling = true;
//                gIsNoPlaceForPrompt = true;
//            }
//            return CRS_SUCCESS;
//         }

        memcpy(&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
        gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;

//        if(memcmp(CLI_PROMPT, _buffer, _size) == 0)
//        {
//            gIsDoneFilling = true;
//        }

        if ((((uint8_t*) _buffer)[_size - 1] == '\r'))
        {
            gIsDoneFilling = true;

        }

        return CRS_SUCCESS;

    }
}

static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size)
{

    if (_size != gWriteNowBuffSize)
    {
        gWriteNowBuffIdx = gWriteNowBuffIdx + _size;
        gWriteNowBuffSize = gWriteNowBuffSize - _size;
        UART_write(gUartHandle, &gWriteNowBuff[gWriteNowBuffIdx],
                   gWriteNowBuffSize);

        return;
    }
    gIsDoneWriting = true;

    if (gIsDoneFilling)
    {

        memset(gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy(gWriteNowBuff, gWriteWaitingBuff, gWriteWaitingBuffIdx);
        gWriteNowBuffSize = gWriteWaitingBuffIdx;
        memset(gWriteWaitingBuff, 0, gWriteWaitingBuffIdx + 1);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
        gIsDoneFilling = false;
        UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);

        return;
    }
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
//        if (gIsTransparentBridge == true)
//        {
//            //CUI_cliPrintf(0, gUartTxBuffer);
//            FPGA_cbArgs_t cbArgs;
//
//            cbArgs.arg3 = _buf;
//            gCbFn(cbArgs);
//            gIsDoneReading = true;
//
//            return 0;
//        }
        gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*) _buf)[0];
        gUartTxBufferIdx++;
        memset(gUartRxBuffer, '\0', _size);

        {
            int i = 0;
            for (i = 0; i <= gUartTxBufferIdx; i++)
            {
                if (memcmp(&(gUartTxBuffer[i]), "AP>", 3) == 0)
                {
//                    gIsDoneReading = true;
                    //CUI_cliPrintf(0, gUartTxBuffer);
                    FPGA_cbArgs_t cbArgs;
                    cbArgs.arg0 = gUartTxBufferIdx;
                    cbArgs.arg1 = gCommandSize;
                    cbArgs.arg3 = (char *)gUartTxBuffer;
                    gCbFn(cbArgs);
                    return;
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

static void processFpgaTimeoutCallback(UArg a0)
{

    Util_setEvent(&Fpga_events, FPGA_OPEN_FAIL_EV);

    Semaphore_post(collectorSem);

}

static void fpgaOpenParsingCallback(const FPGA_cbArgs_t _cbArgs)
{

    Util_setEvent(&Fpga_events, FPGA_OPEN_EV);

    Semaphore_post(collectorSem);

}

static void fpgaTransparentCliParsingCallback(const FPGA_cbArgs_t _cbArgs)
{

    Util_setEvent(&Fpga_events, FPGA_TRANSPARENT_EV);

    Semaphore_post(collectorSem);

}

static void fpgaMultiLineWriteCallback(const FPGA_cbArgs_t _cbArgs)
{

    Util_setEvent(&Fpga_events, FPGA_WRITE_LINES_EV);

    Semaphore_post(collectorSem);
}

static void fpgaMultiLineReadCallback(const FPGA_cbArgs_t _cbArgs)
{

    Util_setEvent(&Fpga_events, FPGA_READ_LINES_EV);

    Semaphore_post(collectorSem);
}


//static CRS_retVal_t fpgaParseCbRsp(char *rsp, uint32_t rspSize)
//{
//    if (rspSize >= 40)
//    {
//        return CRS_FAILURE;
//    }
//    return CRS_SUCCESS;
//}
